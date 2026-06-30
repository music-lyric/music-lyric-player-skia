#!/usr/bin/env python3
"""Generate C++ config code from JSON schema(s).

Per config in `items[]`, emits (in the schema's `namespace`): the resolved `struct`
with defaults, its deeply-optional `Patch` mirror, `applyConfigPatch` (deep-merge),
`diffConfig` (dot-path diff incl. parent paths), and `<Name>Keys` (full-dot-path
`string_view` constants for IDE completion). Output goes to the schema's `file`,
next to the schema, opened with `// clang-format off`.

Schema shape:
    { "namespace": "a::b", "file": "config.gen.h", "items": [
        { "name": "Sub",  "fields": [ { "name": "x", "type": "double", "default"?: "0.0", "desc"?: "..." } ] },
        { "name": "Root", "fields": [ { "name": "sub", "nested": "Sub" } ] } ] }
A leaf field has `type` (+ optional `default`); a nested field has `nested` naming an
earlier config. `desc` is optional.

Usage: `python script/generate-config.py [<schema.json> ...]`. With no args, every
*.schema.json under the project (excluding build / vendored dirs) is generated.
"""

import json
import os
import sys
from pathlib import Path

# Directories never scanned for schemas.
SKIP_DIRS = {"build", "third-party", ".history", ".git", "node_modules", ".turbo", ".vscode", ".claude"}


class SchemaError(Exception):
    """A human-readable problem with the input schema."""


def is_nested(field):
    """Whether a field is a nested child config rather than a leaf value."""
    return "nested" in field


def validate(schema):
    """Reject malformed schemas with a clear message before any code is emitted."""
    if not isinstance(schema, dict):
        raise SchemaError("root must be an object")

    namespace = schema.get("namespace")
    if not isinstance(namespace, str) or not namespace:
        raise SchemaError("'namespace' must be a non-empty string")

    out_file = schema.get("file")
    if not isinstance(out_file, str) or not out_file:
        raise SchemaError("'file' must be a non-empty string (the generated header name)")

    items = schema.get("items")
    if not isinstance(items, list) or not items:
        raise SchemaError("'items' must be a non-empty array")

    defined = set()
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            raise SchemaError(f"items[{index}] must be an object")

        name = item.get("name")
        if not isinstance(name, str) or not name:
            raise SchemaError(f"items[{index}].name must be a non-empty string")
        if name in defined:
            raise SchemaError(f"duplicate config name '{name}'")

        fields = item.get("fields")
        if not isinstance(fields, list):
            raise SchemaError(f"config '{name}'.fields must be an array")

        seen = set()
        for field_index, field in enumerate(fields):
            at = f"config '{name}'.fields[{field_index}]"
            if not isinstance(field, dict):
                raise SchemaError(f"{at} must be an object")

            field_name = field.get("name")
            if not isinstance(field_name, str) or not field_name:
                raise SchemaError(f"{at}.name must be a non-empty string")
            if field_name in seen:
                raise SchemaError(f"duplicate field '{field_name}' in config '{name}'")
            seen.add(field_name)

            if is_nested(field):
                if "type" in field or "default" in field:
                    raise SchemaError(f"{at} ('{field_name}') is nested and must not have 'type'/'default'")
                target = field["nested"]
                if not isinstance(target, str) or not target:
                    raise SchemaError(f"{at} ('{field_name}').nested must be a non-empty string")
                if target not in defined:
                    raise SchemaError(f"{at} ('{field_name}') nests '{target}', which must be defined before '{name}'")
            else:
                field_type = field.get("type")
                if not isinstance(field_type, str) or not field_type:
                    raise SchemaError(f"{at} ('{field_name}') must have a non-empty 'type', or 'nested'")
                if "default" in field and not isinstance(field["default"], str):
                    raise SchemaError(f"{at} ('{field_name}').default must be a string holding a C++ literal")

        defined.add(name)


def jsdoc(desc, indent):
    """Render an optional one-line description as a multi-line JSDoc block."""
    if not desc:
        return ""
    return f"{indent}/**\n{indent} * {desc}\n{indent} */\n"


def note(field):
    """Trailing `// desc` comment for a field, or empty."""
    return f"  // {field['desc']}" if field.get("desc") else ""


def gen_struct(cfg):
    """Emit the resolved struct; leaves carry their defaults (value-initialised when omitted)."""
    out = [jsdoc(cfg.get("desc"), "\t") + f"\tstruct {cfg['name']} {{"]
    for field in cfg["fields"]:
        if is_nested(field):
            out.append(f"\t\t{field['nested']} {field['name']};{note(field)}")
        else:
            default = field.get("default", "{}")
            out.append(f"\t\t{field['type']} {field['name']} = {default};{note(field)}")
    out.append("\t};")
    return "\n".join(out)


def gen_patch(cfg):
    """Emit the deeply-optional patch mirror of a config."""
    out = [f"\tstruct {cfg['name']}Patch {{"]
    for field in cfg["fields"]:
        if is_nested(field):
            out.append(f"\t\t{field['nested']}Patch {field['name']};")
        else:
            out.append(f"\t\t::std::optional<{field['type']}> {field['name']};")
    out.append("\t};")
    return "\n".join(out)


def gen_apply(cfg):
    """Emit applyConfigPatch: deep-merge a patch into a config."""
    name = cfg["name"]
    fields = cfg["fields"]
    unused = "" if fields else "[[maybe_unused]] "
    out = ["\t/**\n\t * Deep-merges a sparse patch into a config.\n\t */"]
    out.append(f"\tinline void applyConfigPatch({unused}{name}& cfg, {unused}const {name}Patch& patch) {{")
    for field in fields:
        n = field["name"]
        if is_nested(field):
            out.append(f"\t\tapplyConfigPatch(cfg.{n}, patch.{n});")
        else:
            out.append(f"\t\tif (patch.{n}.has_value()) {{")
            out.append(f"\t\t\tcfg.{n} = *patch.{n};")
            out.append("\t\t}")
    out.append("\t}")
    return "\n".join(out)


def gen_diff(cfg):
    """Emit diffConfig: collect changed dot-paths; a changed child also records its parent path."""
    name = cfg["name"]
    fields = cfg["fields"]
    unused = "" if fields else "[[maybe_unused]] "
    out = ["\t/**\n\t * Diffs two configs into a dot-path key set; a changed child also records its parent path.\n\t */"]
    out.append(
        f"\tinline void diffConfig({unused}const {name}& prev, {unused}const {name}& next, "
        f"{unused}const ::std::string& prefix, {unused}::music_lyric_player::ChangeKeys& keys) {{"
    )
    for field in fields:
        n = field["name"]
        if is_nested(field):
            out.append("\t\t{")
            out.append("\t\t\tconst ::std::size_t before = keys.size();")
            out.append(f'\t\t\tdiffConfig(prev.{n}, next.{n}, prefix + "{n}" + ".", keys);')
            out.append("\t\t\tif (keys.size() > before) {")
            out.append(f'\t\t\t\tkeys.insert(prefix + "{n}");')
            out.append("\t\t\t}")
            out.append("\t\t}")
        else:
            out.append(f"\t\tif (prev.{n} != next.{n}) {{")
            out.append(f'\t\t\tkeys.insert(prefix + "{n}");')
            out.append("\t\t}")
    out.append("\t}")
    return "\n".join(out)


def gen_keys(cfg, by_name):
    """Emit `namespace <Name>Keys` of full dot-path string_view constants (nested = sub-namespaces)."""
    def emit(fields, prefix, indent):
        lines = []
        for field in fields:
            n = field["name"]
            if is_nested(field):
                lines.append(f"{indent}namespace {n} {{")
                lines += emit(by_name[field["nested"]]["fields"], prefix + n + ".", indent + "\t")
                lines.append(f"{indent}}} // namespace {n}")
            else:
                lines.append(f'{indent}inline constexpr ::std::string_view {n}{{"{prefix + n}"}};')
        return lines

    out = [f"\tnamespace {cfg['name']}Keys {{"]
    out += emit(cfg["fields"], "", "\t\t")
    out.append(f"\t}} // namespace {cfg['name']}Keys")
    return "\n".join(out)


def render(schema, schema_name):
    """Render the whole generated header as a string."""
    namespace = schema["namespace"]
    items = schema["items"]
    by_name = {c["name"]: c for c in items}
    guard = namespace.replace("::", "_").upper() + "_CONFIG_GEN_H_"

    parts = [
        "// clang-format off",
        f"// Generated by script/generate-config.py from {schema_name}. DO NOT EDIT.",
        "// To change config, edit the schema and regenerate with script/generate-config.py.",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <cstddef>",
        "#include <optional>",
        "#include <string>",
        "#include <string_view>",
        "",
        '#include "utils/manager/config.h"',
        "",
        f"namespace {namespace} {{",
    ]

    # structs, then patches, then merge, then diff, then key trees
    for stage in (gen_struct, gen_patch, gen_apply, gen_diff):
        for cfg in items:
            parts.append(stage(cfg))
            parts.append("")
    for cfg in items:
        parts.append(gen_keys(cfg, by_name))
        parts.append("")

    parts.append(f"}} // namespace {namespace}")
    parts.append("")
    parts.append(f"#endif // {guard}")
    parts.append("// clang-format on")
    parts.append("")
    return "\n".join(parts)


def scan_schemas(root):
    """Find every *.schema.json under root, pruning build / vendored directories."""
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.endswith(".schema.json"):
                found.append(Path(dirpath) / name)
    return sorted(found)


def generate_one(schema_path):
    """Read, validate and generate a single schema; returns 0 on success, 1 on error."""
    try:
        raw = schema_path.read_text(encoding="utf-8")
    except OSError as error:
        print(f"error: cannot read {schema_path}: {error}", file=sys.stderr)
        return 1

    try:
        schema = json.loads(raw)
    except json.JSONDecodeError as error:
        print(f"error: invalid JSON in {schema_path}: {error}", file=sys.stderr)
        return 1

    try:
        validate(schema)
    except SchemaError as error:
        print(f"error: {schema_path}: {error}", file=sys.stderr)
        return 1

    out_path = schema_path.parent / schema["file"]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render(schema, schema_path.name), encoding="utf-8")
    print(f"generated {out_path} from {schema_path}")
    return 0


def main(argv):
    given = argv[1:]
    if given:
        schemas = [Path(a) for a in given]
        missing = [str(p) for p in schemas if not p.is_file()]
        if missing:
            print("error: schema not found: " + ", ".join(missing), file=sys.stderr)
            return 1
    else:
        root = Path(__file__).resolve().parent.parent
        schemas = scan_schemas(root)
        if not schemas:
            print(f"error: no *.schema.json found under {root}", file=sys.stderr)
            return 1
        print(f"scanned {root}: {len(schemas)} schema(s)")

    status = 0
    for schema_path in schemas:
        status |= generate_one(schema_path)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
