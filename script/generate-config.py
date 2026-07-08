#!/usr/bin/env python3
"""Generate C++ config code from JSON schema(s).

Per config in `items[]`, emits (in the schema's `namespace`): the resolved `struct`
with defaults, its deeply-optional `Patch` mirror, `applyConfigPatch` (deep-merge),
`diffConfig` (dot-path diff incl. parent paths), and `<Name>Keys` (full-dot-path
`string_view` constants for IDE completion). A config named `Root` additionally gets an
`inline const Root Default` instance (all schema defaults) so consumers can fall back to a default
value on a parse failure. Output goes to the schema's `file`, next to the schema, opened with
`// clang-format off`.

Schema shape:
    { "namespace": "a::b", "file": "config.gen.h",
      "imports"?: [ { "as": "alias", "schema": "other.schema.json" } ],
      "items": [
        { "name": "Sub",  "fields": [ { "name": "x", "type": "double", "default"?: "0.0",
                                        "example"?: [1, 2], "min"?: 0, "max"?: 10, "comment"?: "..." } ] },
        { "name": "Root", "fields": [ { "name": "sub", "nested": "Sub", "defaults"?: { "x": "1.0" } },
                                      { "name": "ext", "nested": "alias.Root" } ] } ] }
A leaf field has `type` (+ optional `default`, a C++ literal string), `kind`
(`"color"` / `"length"` / `"easing"`) which implies `type: ::std::string` and appends a standard
format note to the doc, or `enum` (+ `values`, + optional `enumComment`) which emits an `enum class`
and types the field as it (`default` is then an enumerator name). A nested field has `nested`
naming a config (+ optional `defaults`, a map of sub-field -> C++ literal overriding that nesting's
defaults via designated initialisers). A bare name (`"Sub"`) targets an earlier config in the same
schema; a dotted name (`"alias.Root"`) targets a config from an imported schema, referenced by its
fully-qualified type and pulled in with an `#include`.

`comment` (a string, or a list of lines) is optional documentation. On a leaf with a list comment (or
none), the generator auto-appends `@default` / `@example` / `@minimum` / `@maximum` JSDoc lines from
the leaf's `default` / `example` (scalar or list) / `min` / `max`; a string comment stays an inline
`// ...` and gets no tags.

Usage: `python script/generate-config.py [<schema.json> ...]`. With no args, every
*.schema.json under the project (excluding build / vendored dirs) is generated.
"""

import json
import os
import sys
from pathlib import Path

# Directories never scanned for schemas.
SKIP_DIRS = {"build", "third-party", ".history", ".git", "node_modules", ".turbo", ".vscode", ".claude"}

# Standard format notes appended to a `kind`-typed leaf's doc, so schemas never repeat the format prose.
KIND_NOTES = {
    "color": "Accepts `#RGB` / `#RGBA` / `#RRGGBB` / `#RRGGBBAA`, `rgb(r, g, b)` or `rgba(r, g, b, a)` (a in 0..1).",
    "length": "A bare number or `px` is an absolute length in logical pixels.",
    "easing": "Accepts `linear`, `ease`, `ease-in`, `ease-out`, `ease-in-out` or `cubic-bezier(x1, y1, x2, y2)`.",
}


class SchemaError(Exception):
    """A human-readable problem with the input schema."""


class Module:
    """A loaded schema plus its resolved imports, ready to render or reference."""

    def __init__(self, path, schema):
        self.path = path
        self.namespace = schema["namespace"]
        self.file = schema["file"]
        self.inline = schema.get("inline", False)
        self.items = schema["items"]
        self.by_name = {c["name"]: c for c in schema["items"]}
        self.imports = {}  # alias -> Module


def is_nested(field):
    """Whether a field is a nested child config rather than a leaf value."""
    return "nested" in field


def leaf_type(field):
    """C++ type of a leaf field: `enum` names its enum class, `kind` implies `::std::string`, otherwise the explicit `type`."""
    if "enum" in field:
        return field["enum"]
    return "::std::string" if "kind" in field else field["type"]


def leaf_default(field):
    """The C++ default initialiser for a leaf: `Enum::Value` for enums, the raw literal otherwise (`{}` when unset)."""
    if "enum" in field:
        return f"{field['enum']}::{field['default']}" if "default" in field else "{}"
    return field.get("default", "{}")


def is_valid_comment(value):
    """Whether a `comment` is a string or a list of strings."""
    return isinstance(value, str) or (isinstance(value, list) and all(isinstance(line, str) for line in value))


def validate_structure(schema):
    """Reject a malformed schema with a clear message; dotted `nested` targets are checked in `validate_refs`."""
    if not isinstance(schema, dict):
        raise SchemaError("root must be an object")

    namespace = schema.get("namespace")
    if not isinstance(namespace, str) or not namespace:
        raise SchemaError("'namespace' must be a non-empty string")

    out_file = schema.get("file")
    if not isinstance(out_file, str) or not out_file:
        raise SchemaError("'file' must be a non-empty string (the generated header name)")

    if "inline" in schema and not isinstance(schema["inline"], bool):
        raise SchemaError("'inline' must be a boolean (fields render as inline `// ...` with no tags)")

    imports = schema.get("imports", [])
    if not isinstance(imports, list):
        raise SchemaError("'imports' must be an array")
    aliases = set()
    for index, imp in enumerate(imports):
        if not isinstance(imp, dict):
            raise SchemaError(f"imports[{index}] must be an object")
        alias = imp.get("as")
        if not isinstance(alias, str) or not alias:
            raise SchemaError(f"imports[{index}].as must be a non-empty string")
        if alias in aliases:
            raise SchemaError(f"duplicate import alias '{alias}'")
        aliases.add(alias)
        target = imp.get("schema")
        if not isinstance(target, str) or not target:
            raise SchemaError(f"import '{alias}'.schema must be a non-empty string (a *.schema.json path)")

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
        if "comment" in item and not is_valid_comment(item["comment"]):
            raise SchemaError(f"config '{name}'.comment must be a string or a list of strings")

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

            if "comment" in field and not is_valid_comment(field["comment"]):
                raise SchemaError(f"{at} ('{field_name}').comment must be a string or a list of strings")

            for bound in ("min", "max"):
                if bound in field and not isinstance(field[bound], (int, float)):
                    raise SchemaError(f"{at} ('{field_name}').{bound} must be a number")
            if "example" in field:
                examples = field["example"] if isinstance(field["example"], list) else [field["example"]]
                if not examples or not all(isinstance(e, (str, int, float, bool)) for e in examples):
                    raise SchemaError(f"{at} ('{field_name}').example must be a scalar or a non-empty list of scalars")

            if is_nested(field):
                if any(key in field for key in ("type", "kind", "enum", "values", "default", "example", "min", "max")):
                    raise SchemaError(f"{at} ('{field_name}') is nested and must not have 'type'/'kind'/'enum'/'values'/'default'/'example'/'min'/'max'")
                overrides = field.get("defaults")
                if overrides is not None and not (
                    isinstance(overrides, dict) and overrides and all(isinstance(k, str) and isinstance(v, str) for k, v in overrides.items())
                ):
                    raise SchemaError(f"{at} ('{field_name}').defaults must be a non-empty object of field -> C++ literal string")
                target = field["nested"]
                if not isinstance(target, str) or not target:
                    raise SchemaError(f"{at} ('{field_name}').nested must be a non-empty string")
                if "." in target:
                    alias = target.split(".", 1)[0]
                    if alias not in aliases:
                        raise SchemaError(f"{at} ('{field_name}') nests '{target}', but import '{alias}' is not declared")
                elif target not in defined:
                    raise SchemaError(f"{at} ('{field_name}') nests '{target}', which must be defined before '{name}'")
            else:
                enum_name = field.get("enum")
                kind = field.get("kind")
                if enum_name is not None:
                    if not isinstance(enum_name, str) or not enum_name:
                        raise SchemaError(f"{at} ('{field_name}').enum must be a non-empty string (the enum class name)")
                    if "type" in field or kind is not None:
                        raise SchemaError(f"{at} ('{field_name}') has 'enum' and must not also set 'type'/'kind'")
                    values = field.get("values")
                    if not (isinstance(values, list) and values and all(isinstance(v, str) and v for v in values)):
                        raise SchemaError(f"{at} ('{field_name}').values must be a non-empty array of enumerator name strings")
                    if len(set(values)) != len(values):
                        raise SchemaError(f"{at} ('{field_name}').values has duplicate enumerator names")
                    if "default" in field and field["default"] not in values:
                        raise SchemaError(f"{at} ('{field_name}').default must be one of 'values'")
                    if "enumComment" in field and not is_valid_comment(field["enumComment"]):
                        raise SchemaError(f"{at} ('{field_name}').enumComment must be a string or a list of strings")
                elif kind is not None:
                    if kind not in KIND_NOTES:
                        raise SchemaError(f"{at} ('{field_name}').kind must be one of {sorted(KIND_NOTES)}")
                    if "type" in field:
                        raise SchemaError(f"{at} ('{field_name}') has 'kind' and must not also set 'type' (kind implies ::std::string)")
                else:
                    field_type = field.get("type")
                    if not isinstance(field_type, str) or not field_type:
                        raise SchemaError(f"{at} ('{field_name}') must have a non-empty 'type', a 'kind', an 'enum', or 'nested'")
                if "default" in field and not isinstance(field["default"], str):
                    raise SchemaError(f"{at} ('{field_name}').default must be a string holding a C++ literal")

        defined.add(name)


def validate_refs(module):
    """Verify that every dotted `nested` target resolves to a struct in the named import."""
    for cfg in module.items:
        for field in cfg["fields"]:
            if not is_nested(field) or "." not in field["nested"]:
                continue
            alias, _, struct = field["nested"].partition(".")
            target = module.imports.get(alias)
            if target is None:
                raise SchemaError(f"config '{cfg['name']}' nests '{field['nested']}', but import '{alias}' failed to load")
            if struct not in target.by_name:
                raise SchemaError(f"config '{cfg['name']}' nests '{field['nested']}', but '{struct}' is not defined in that schema")


def load_module(path, cache, loading=None):
    """Read, validate and resolve a schema (and its imports) into a Module, memoised by path."""
    key = path.resolve()
    if key in cache:
        return cache[key]
    loading = loading if loading is not None else set()
    if key in loading:
        raise SchemaError(f"import cycle detected at {path}")
    loading.add(key)

    try:
        raw = path.read_text(encoding="utf-8")
    except OSError as error:
        raise SchemaError(f"cannot read {path}: {error}")
    try:
        schema = json.loads(raw)
    except json.JSONDecodeError as error:
        raise SchemaError(f"invalid JSON in {path}: {error}")

    validate_structure(schema)
    module = Module(path, schema)

    for imp in schema.get("imports", []):
        imp_path = (path.parent / imp["schema"]).resolve()
        if not imp_path.is_file():
            raise SchemaError(f"import '{imp['as']}' of {path.name} points to missing schema {imp_path}")
        module.imports[imp["as"]] = load_module(imp_path, cache, loading)

    validate_refs(module)
    loading.discard(key)
    cache[key] = module
    return module


def resolve_nested(module, value):
    """Resolve a `nested` reference (bare = local & unqualified, dotted `alias.Struct` = imported & fully-qualified) to `(module, cfg, type, patch_type)`."""
    if "." in value:
        alias, _, struct = value.partition(".")
        target = module.imports[alias]
        type_str = f"::{target.namespace}::{struct}"
        return target, target.by_name[struct], type_str, type_str + "Patch"
    return module, module.by_name[value], value, value + "Patch"


def include_path_for(out_path):
    """Project-relative include path for a generated header (relative to the nearest `src/`)."""
    parts = out_path.resolve().parts
    if "src" in parts:
        cut = len(parts) - 1 - parts[::-1].index("src")
        return "/".join(parts[cut + 1:])
    return out_path.name


def referenced_includes(module):
    """Include paths for every imported header actually referenced by a nested field."""
    paths = set()
    for cfg in module.items:
        for field in cfg["fields"]:
            if is_nested(field) and "." in field["nested"]:
                target = module.imports[field["nested"].split(".", 1)[0]]
                paths.add(include_path_for(target.path.parent / target.file))
    return sorted(paths)


def doc_lines(desc):
    """Normalise a `desc` (absent, a string, or a list of strings) into a list of lines."""
    if desc is None:
        return []
    return list(desc) if isinstance(desc, list) else [desc]


def jsdoc(desc, indent):
    """Render an optional description (a string or a list of lines) as a multi-line JSDoc block."""
    lines = doc_lines(desc)
    if not lines:
        return ""
    body = "\n".join(f"{indent} * {line}" if line else f"{indent} *" for line in lines)
    return f"{indent}/**\n{body}\n{indent} */\n"


def field_tags(field):
    """The `@default` / `@example` / `@minimum` / `@maximum` JSDoc lines derived from a field's structured metadata."""

    def render(value):
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, str):
            return f'"{value}"'
        return str(value)

    lines = []
    if "default" in field:
        default = f"{field['enum']}::{field['default']}" if "enum" in field else field["default"]
        lines.append(f"@default {default}")  # a C++/display literal (`Enum::Value` for enums), emitted verbatim.
    example = field.get("example")
    for item in example if isinstance(example, list) else ([] if example is None else [example]):
        lines.append(f"@example {render(item)}")
    if "min" in field:
        lines.append(f"@minimum {render(field['min'])}")
    if "max" in field:
        lines.append(f"@maximum {render(field['max'])}")
    return lines


def field_doc(field, indent, inline):
    """Block-style JSDoc above a field: its `comment` lines, the `kind` format note, then the auto `@default`/`@example`/... tags. Inline modules use `note` instead."""
    if inline:
        return ""
    lines = doc_lines(field.get("comment"))
    kind = field.get("kind")
    if kind:
        lines.append(KIND_NOTES[kind])
    tags = field_tags(field)
    if tags:
        if lines and lines[-1] != "":
            lines.append("")
        lines += tags
    return jsdoc(lines, indent)


def note(field, inline):
    """Trailing `// comment` for an inline-module field with a string comment, or empty."""
    comment = field.get("comment")
    return f"  // {comment}" if inline and isinstance(comment, str) and comment else ""


def gen_struct(cfg, module):
    """Emit the resolved struct; leaves carry their defaults (value-initialised when omitted)."""
    out = [jsdoc(cfg.get("comment"), "\t") + f"\tstruct {cfg['name']} {{"]
    for field in cfg["fields"]:
        above = field_doc(field, "\t\t", module.inline)
        if is_nested(field):
            _, _, type_str, _ = resolve_nested(module, field["nested"])
            overrides = field.get("defaults")
            if overrides:
                # C++20 designated initialisers override chosen sub-fields; the rest keep their in-class defaults.
                inits = ", ".join(f".{key} = {value}" for key, value in overrides.items())
                out.append(f"{above}\t\t{type_str} {field['name']} = {type_str}{{ {inits} }};{note(field, module.inline)}")
            else:
                out.append(f"{above}\t\t{type_str} {field['name']};{note(field, module.inline)}")
        else:
            out.append(f"{above}\t\t{leaf_type(field)} {field['name']} = {leaf_default(field)};{note(field, module.inline)}")
    out.append("\t};")
    return "\n".join(out)


def gen_patch(cfg, module):
    """Emit the deeply-optional patch mirror of a config."""
    out = [f"\tstruct {cfg['name']}Patch {{"]
    for field in cfg["fields"]:
        if is_nested(field):
            _, _, _, patch_str = resolve_nested(module, field["nested"])
            out.append(f"\t\t{patch_str} {field['name']};")
        else:
            out.append(f"\t\t::std::optional<{leaf_type(field)}> {field['name']};")
    out.append("\t};")
    return "\n".join(out)


def gen_apply(cfg, module):
    """Emit applyConfigPatch: deep-merge a patch into a config (nested calls resolve via ADL)."""
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


def gen_diff(cfg, module):
    """Emit diffConfig: collect changed dot-paths; a changed child also records its parent path."""
    name = cfg["name"]
    fields = cfg["fields"]
    unused = "" if fields else "[[maybe_unused]] "
    out = ["\t/**\n\t * Diffs two configs into a dot-path key set; a changed child also records its parent path.\n\t */"]
    out.append(
        f"\tinline void diffConfig({unused}const {name}& prev, {unused}const {name}& next, "
        f"{unused}const ::std::string& prefix, {unused}::music_lyric_player::utils::config::ChangeKeys& keys) {{"
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


def gen_keys(cfg, module):
    """Emit `namespace <Name>Keys` of full dot-path string_view constants (nested = sub-namespaces)."""
    def emit(fields, prefix, indent, mod):
        lines = []
        for field in fields:
            n = field["name"]
            if is_nested(field):
                target_mod, target_cfg, _, _ = resolve_nested(mod, field["nested"])
                lines.append(f"{indent}namespace {n} {{")
                lines += emit(target_cfg["fields"], prefix + n + ".", indent + "\t", target_mod)
                lines.append(f"{indent}}} // namespace {n}")
            else:
                lines.append(f'{indent}inline constexpr ::std::string_view {n}{{"{prefix + n}"}};')
        return lines

    out = [f"\tnamespace {cfg['name']}Keys {{"]
    out += emit(cfg["fields"], "", "\t\t", module)
    out.append(f"\t}} // namespace {cfg['name']}Keys")
    return "\n".join(out)


def collect_enums(module):
    """Ordered `{enum_name: (values, comment)}` across every leaf field in the module; errors on a conflicting redefinition."""
    enums = {}
    for cfg in module.items:
        for field in cfg["fields"]:
            if is_nested(field) or "enum" not in field:
                continue
            name = field["enum"]
            values = tuple(field["values"])
            if name in enums and enums[name][0] != values:
                raise SchemaError(f"enum '{name}' has conflicting 'values' across fields in {module.path.name}")
            enums.setdefault(name, (values, field.get("enumComment")))
    return enums


def gen_enums(module):
    """The `enum class` blocks for the module's enum fields, emitted before the structs that reference them."""
    blocks = []
    for name, (values, comment) in collect_enums(module).items():
        lines = [jsdoc(comment, "\t") + f"\tenum class {name} {{"]
        lines += [f"\t\t{value}," for value in values]
        lines.append("\t};")
        blocks.append("\n".join(lines))
    return blocks


def gen_default(cfg):
    """Emit `inline const Root Default` (every field at its schema default) for a `Root` config, so consumers can fall back to it; empty otherwise."""
    if cfg["name"] != "Root":
        return ""
    return "\n".join([
        "\t/**",
        "\t * Built-in defaults (every field at its schema default); the fallback when a config value fails to parse.",
        "\t */",
        "\tinline const Root Default{};",
    ])


def render(module, schema_name):
    """Render the whole generated header as a string."""
    namespace = module.namespace
    items = module.items
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
    ]
    parts += [f'#include "{path}"' for path in referenced_includes(module)]
    parts += [
        "",
        f"namespace {namespace} {{",
    ]

    # enums first (referenced by the structs below), then structs / patches / merge / diff / key trees / the `Root` default instance
    for block in gen_enums(module):
        parts.append(block)
        parts.append("")
    for stage in (gen_struct, gen_patch, gen_apply, gen_diff):
        for cfg in items:
            parts.append(stage(cfg, module))
            parts.append("")
    for cfg in items:
        parts.append(gen_keys(cfg, module))
        parts.append("")
    for cfg in items:
        block = gen_default(cfg)
        if block:
            parts.append(block)
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
    """Load, validate and generate a single schema (resolving imports); 0 on success, 1 on error."""
    try:
        module = load_module(schema_path, cache={})
    except SchemaError as error:
        print(f"error: {schema_path}: {error}", file=sys.stderr)
        return 1

    out_path = schema_path.parent / module.file
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render(module, schema_path.name), encoding="utf-8")
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
