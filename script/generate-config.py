#!/usr/bin/env python3
"""Generate C++ config code from JSON schema(s).

Per config in `items[]`, emits (in the schema's `namespace`) a single resolved `struct`: each leaf is a
`::music_lyric_player::utils::config::Property<T>` carrying the schema default plus a presence flag, so the
one struct doubles as its own sparse patch (a `Property` reads through to its value but also records whether
it was explicitly set). It also gets a defaulted `operator==` (values only) and the config machinery
`overlay` (copy every set leaf from one config onto another) and `capture` (record changed leaves into a
delta config) as hidden `friend`s, each taking a trailing `::music_lyric_player::utils::config::Access`
passkey. Because the machinery is a hidden friend it never leaks into the namespace (found solely via ADL)
and the passkey's private constructor means only `Manager` can invoke it; `Manager` reports changes with a
delta config from `capture`. A schema with `"defaultInstance": true` additionally gets an
`inline const Root Default` instance (all schema defaults) for its `Root` plus a hidden-friend
`resolve(out, overrides, key)` that resets `out` to the defaults, overlays the accumulated overrides, then
fills every leaf that was not explicitly set from its inherited source, so consumers can fall back to a
default value on a parse failure. Output goes to the schema's `file`, next to the schema, opened with
`// clang-format off`.

Schema shape:
    { "namespace": "a::b", "file": "config.gen.h", "defaultInstance"?: true,
      "imports"?: [ { "as": "alias", "schema": "other.schema.json" } ],
      "mix"?: { "shared": [ { "name": "duration", "type": "double", "default": "500.0" } ] },
      "items": [
        { "name": "Sub",  "fields": [ { "name": "x", "type": "double", "default"?: "0.0",
                                        "example"?: [1, 2], "min"?: 0, "max"?: 10, "comment"?: "..." } ] },
        { "name": "Root", "fields": [ { "include": "shared", "defaults"?: { "duration": "600.0" } },
                                      { "name": "sub", "nested": "Sub", "defaults"?: { "x": "1.0" } },
                                      { "name": "ext", "nested": "alias.Root" } ] } ] }
A schema-local `mix` maps reusable names to field arrays. A field entry `{ "include": "name" }`
expands that mix in place before validation and generation; optional `defaults` replaces selected leaf
default literals in that inclusion. Mixes cannot include other mixes and every declared mix must be used.
A top-level `"root": { "comment"?: ..., "modules": [ { "name": "x", "schema": "x.schema.json", "comment"?: ... } ] }`
expands (before validation) into one import per module plus a generated `Root` config nesting each module's
own `Root`, so a pure aggregate layer declares its composition once instead of hand-writing the parallel
`imports` + `Root` item; a schema using `root` must not also define its own `Root` item.
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

import copy
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

# Passkey type gating the generated machinery; only `utils::config::Manager` can mint one, so external code cannot call overlay / capture / resolve.
ACCESS_TYPE = "::music_lyric_player::utils::config::Access"

# Project-relative include for the passkey header, added to every generated header.
ACCESS_INCLUDE = "utils/config/access.h"

# Wrapper carrying each leaf's value plus its presence flag, so one Config struct doubles as its own patch.
PROPERTY_TYPE = "::music_lyric_player::utils::config::Property"

# Project-relative include for the Property wrapper, added to every generated header.
PROPERTY_INCLUDE = "utils/config/property.h"


class SchemaError(Exception):
    """A human-readable problem with the input schema."""


class Module:
    """A loaded schema plus its resolved imports, ready to render or reference."""

    def __init__(self, path, schema):
        self.path = path
        self.namespace = schema["namespace"]
        self.file = schema["file"]
        self.inline = schema.get("inline", False)
        self.default_instance = schema.get("defaultInstance", False)
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


def expand_mix(schema):
    """Expand schema-local `{ "include": "..." }` field entries from `mix`, applying optional leaf-default overrides."""
    if not isinstance(schema, dict):
        return schema

    mixes = schema.get("mix", {})
    if not isinstance(mixes, dict):
        raise SchemaError("'mix' must be an object mapping names to field arrays")

    mix_fields = {}
    for name, fields in mixes.items():
        if not isinstance(name, str) or not name:
            raise SchemaError("every 'mix' key must be a non-empty string")
        if not isinstance(fields, list) or not fields:
            raise SchemaError(f"mix '{name}' must be a non-empty field array")

        seen = set()
        for index, field in enumerate(fields):
            at = f"mix '{name}'[{index}]"
            if not isinstance(field, dict):
                raise SchemaError(f"{at} must be an object")
            if "include" in field:
                raise SchemaError(f"{at} must not include another mix")
            field_name = field.get("name")
            if not isinstance(field_name, str) or not field_name:
                raise SchemaError(f"{at}.name must be a non-empty string")
            if field_name in seen:
                raise SchemaError(f"duplicate field '{field_name}' in mix '{name}'")
            seen.add(field_name)
        mix_fields[name] = fields

    normalized = copy.deepcopy(schema)
    used = set()
    items = normalized.get("items")
    if isinstance(items, list):
        for item_index, item in enumerate(items):
            if not isinstance(item, dict) or not isinstance(item.get("fields"), list):
                continue
            expanded = []
            for field_index, field in enumerate(item["fields"]):
                if not isinstance(field, dict) or "include" not in field:
                    expanded.append(field)
                    continue

                at = f"items[{item_index}].fields[{field_index}]"
                extra = set(field) - {"include", "defaults"}
                if extra:
                    raise SchemaError(f"{at} includes a mix and must not also set {sorted(extra)}")
                name = field["include"]
                if not isinstance(name, str) or not name:
                    raise SchemaError(f"{at}.include must be a non-empty string")
                if name not in mix_fields:
                    raise SchemaError(f"{at} includes unknown mix '{name}'")

                defaults = field.get("defaults", {})
                if not isinstance(defaults, dict) or ("defaults" in field and not defaults) or not all(isinstance(k, str) and isinstance(v, str) for k, v in defaults.items()):
                    raise SchemaError(f"{at}.defaults must be a non-empty object of field -> C++ literal string")
                available = {mixed["name"] for mixed in mix_fields[name]}
                unknown = set(defaults) - available
                if unknown:
                    raise SchemaError(f"{at}.defaults overrides unknown fields {sorted(unknown)} in mix '{name}'")

                for mixed in mix_fields[name]:
                    clone = copy.deepcopy(mixed)
                    if clone["name"] in defaults:
                        if is_nested(clone):
                            raise SchemaError(f"{at}.defaults cannot override nested mix field '{clone['name']}'")
                        clone["default"] = defaults[clone["name"]]
                    expanded.append(clone)
                used.add(name)
            item["fields"] = expanded

    unused = set(mix_fields) - used
    if unused:
        raise SchemaError(f"unused mix definitions: {sorted(unused)}")
    return normalized


def expand_root(schema):
    """Expand a top-level `root` entry (an ordered module list) into imports plus a generated `Root`.

    Each module `{ "name": n, "schema": path, "comment"?: ... }` becomes an import aliased `n` and a
    `Root` field nesting that schema's own `Root`, so a pure aggregate layer declares its composition
    once instead of hand-writing the parallel `imports` + `Root` item.
    """
    if not isinstance(schema, dict) or "root" not in schema:
        return schema

    root = schema["root"]
    if not isinstance(root, dict):
        raise SchemaError("'root' must be an object with a 'modules' array")
    modules = root.get("modules")
    if not isinstance(modules, list) or not modules:
        raise SchemaError("'root.modules' must be a non-empty array")

    normalized = copy.deepcopy(schema)
    imports = normalized.get("imports")
    if not isinstance(imports, list):
        imports = []
    aliases = {imp["as"] for imp in imports if isinstance(imp, dict) and isinstance(imp.get("as"), str)}

    fields = []
    seen = set()
    for index, module in enumerate(modules):
        at = f"root.modules[{index}]"
        if not isinstance(module, dict):
            raise SchemaError(f"{at} must be an object")
        name = module.get("name")
        if not isinstance(name, str) or not name:
            raise SchemaError(f"{at}.name must be a non-empty string")
        if name in seen:
            raise SchemaError(f"duplicate root module '{name}'")
        seen.add(name)
        target = module.get("schema")
        if not isinstance(target, str) or not target:
            raise SchemaError(f"{at}.schema must be a non-empty string (a *.schema.json path)")
        if name in aliases:
            raise SchemaError(f"root module '{name}' clashes with an existing import alias '{name}'")
        imports.append({"as": name, "schema": target})
        field = {"name": name, "nested": f"{name}.Root"}
        if "comment" in module:
            field["comment"] = module["comment"]
        fields.append(field)

    items = normalized.get("items")
    if not isinstance(items, list):
        items = []
    if any(isinstance(item, dict) and item.get("name") == "Root" for item in items):
        raise SchemaError("a schema with 'root' must not also define a 'Root' item")
    root_item = {"name": "Root", "fields": fields}
    if "comment" in root:
        root_item["comment"] = root["comment"]
    items.append(root_item)

    normalized["imports"] = imports
    normalized["items"] = items
    del normalized["root"]
    return normalized


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

    if "defaultInstance" in schema and not isinstance(schema["defaultInstance"], bool):
        raise SchemaError("'defaultInstance' must be a boolean (emit the `Root` Default instance for this schema)")

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
                if "inheritFrom" in field and (not isinstance(field["inheritFrom"], str) or not field["inheritFrom"]):
                    raise SchemaError(f"{at} ('{field_name}').inheritFrom must be a non-empty string (a dotted path from Root to the inherited source)")
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
                if "inheritFrom" in field:
                    raise SchemaError(f"{at} ('{field_name}') has 'inheritFrom' but is a leaf; inheritance is only supported on nested fields")
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

    schema = expand_mix(schema)
    schema = expand_root(schema)
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
    """Resolve a `nested` reference (bare = local & unqualified, dotted `alias.Struct` = imported & fully-qualified) to `(module, cfg, type)`."""
    if "." in value:
        alias, _, struct = value.partition(".")
        target = module.imports[alias]
        type_str = f"::{target.namespace}::{struct}"
        return target, target.by_name[struct], type_str
    return module, module.by_name[value], value


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
    """Emit the resolved struct; leaves carry their defaults (value-initialised when omitted), and the config machinery rides along as hidden friends."""
    out = [jsdoc(cfg.get("comment"), "\t") + f"\tstruct {cfg['name']} {{"]
    for field in cfg["fields"]:
        above = field_doc(field, "\t\t", module.inline)
        if is_nested(field):
            _, _, type_str = resolve_nested(module, field["nested"])
            overrides = field.get("defaults")
            if overrides:
                # C++20 designated initialisers override chosen sub-fields; the rest keep their in-class defaults.
                inits = ", ".join(f".{key} = {value}" for key, value in overrides.items())
                out.append(f"{above}\t\t{type_str} {field['name']} = {type_str}{{ {inits} }};{note(field, module.inline)}")
            else:
                out.append(f"{above}\t\t{type_str} {field['name']};{note(field, module.inline)}")
        else:
            out.append(f"{above}\t\t{PROPERTY_TYPE}<{leaf_type(field)}> {field['name']} = {leaf_default(field)};{note(field, module.inline)}")
    # A defaulted equality lets Manager tell whether a resolve actually changed anything; the rest of the machinery is hidden friends found only via ADL and gated by the passkey, so they stay out of the namespace and only Manager can invoke them.
    out.append("")
    out.append(f"\t\tbool operator==(const {cfg['name']}&) const = default;")
    out.append("")
    out.append(gen_overlay(cfg, module))
    out.append("")
    out.append(gen_capture(cfg, module))
    if module.default_instance and cfg["name"] == "Root":
        out.append("")
        out.append(gen_resolve(module))
    out.append("\t};")
    return "\n".join(out)


def gen_overlay(cfg, module):
    """Emit the hidden-friend `overlay`: copy every explicitly-set leaf from `src` onto `dst`; nested calls resolve via ADL and forward the passkey."""
    name = cfg["name"]
    fields = cfg["fields"]
    obj = "" if fields else "[[maybe_unused]] "
    key = "" if any(is_nested(field) for field in fields) else "[[maybe_unused]] "
    out = [f"\t\tfriend void overlay({obj}{name}& dst, {obj}const {name}& src, {key}{ACCESS_TYPE} key) {{"]
    for field in fields:
        n = field["name"]
        if is_nested(field):
            out.append(f"\t\t\toverlay(dst.{n}, src.{n}, key);")
        else:
            out.append(f"\t\t\tif (src.{n}.assigned()) dst.{n} = src.{n}.value();")
    out.append("\t\t}")
    return "\n".join(out)


def gen_capture(cfg, module):
    """Emit the hidden-friend `capture`: record every leaf whose value changed between `prev` and `next` into `delta` (nested calls resolve via ADL and forward the passkey)."""
    name = cfg["name"]
    fields = cfg["fields"]
    obj = "" if fields else "[[maybe_unused]] "
    key = "" if any(is_nested(field) for field in fields) else "[[maybe_unused]] "
    out = [f"\t\tfriend void capture({obj}{name}& delta, {obj}const {name}& prev, {obj}const {name}& next, {key}{ACCESS_TYPE} key) {{"]
    for field in fields:
        n = field["name"]
        if is_nested(field):
            out.append(f"\t\t\tcapture(delta.{n}, prev.{n}, next.{n}, key);")
        else:
            out.append(f"\t\t\tif (!(prev.{n} == next.{n})) delta.{n} = next.{n}.value();")
    out.append("\t\t}")
    return "\n".join(out)


def walk_leaves(module, cfg, prefix=""):
    """Every leaf's dotted sub-path under a config, recursing through nested fields (imported or local)."""
    leaves = []
    for field in cfg["fields"]:
        path = f"{prefix}{field['name']}"
        if is_nested(field):
            target_module, target_cfg, _ = resolve_nested(module, field["nested"])
            leaves += walk_leaves(target_module, target_cfg, path + ".")
        else:
            leaves.append(path)
    return leaves


def resolve_path(root_module, path):
    """Resolve a dotted absolute path from `Root` to the `(module, cfg)` of the nested config it names; raises if a segment is missing or a leaf."""
    module, cfg = root_module, root_module.by_name["Root"]
    for part in path.split("."):
        field = next((f for f in cfg["fields"] if f["name"] == part), None)
        if field is None:
            raise SchemaError(f"inheritFrom path '{path}' has no field '{part}'")
        if not is_nested(field):
            raise SchemaError(f"inheritFrom path '{path}' segment '{part}' is a leaf, not a nested config")
        module, cfg, _ = resolve_nested(module, field["nested"])
    return module, cfg


def collect_inherit_edges(root_module):
    """Walk the whole tree from `Root`, returning `(consumer_path, source_path, type_module, type_cfg)` for every `inheritFrom` field; validates each source resolves to the same type and is not the field itself or its own descendant."""
    edges = []

    def walk(module, cfg, prefix):
        for field in cfg["fields"]:
            if not is_nested(field):
                continue
            path = f"{prefix}{field['name']}"
            target_module, target_cfg, _ = resolve_nested(module, field["nested"])
            if "inheritFrom" in field:
                source = field["inheritFrom"]
                if source == path or source.startswith(path + "."):
                    raise SchemaError(f"field '{path}' cannot inheritFrom '{source}' (itself or its own descendant)")
                source_module, source_cfg = resolve_path(root_module, source)
                if (source_module.namespace, source_cfg["name"]) != (target_module.namespace, target_cfg["name"]):
                    raise SchemaError(f"inheritFrom '{source}' resolves to '{source_cfg['name']}' but field '{path}' is '{target_cfg['name']}'; the inherited source must be the same type")
                edges.append((path, source, target_module, target_cfg))
            walk(target_module, target_cfg, path + ".")

    walk(root_module, root_module.by_name["Root"], "")
    return order_inherit_edges(edges)


def order_inherit_edges(edges):
    """Order the fix-ups so every source is fully resolved before it is read (a consumer whose source lies inside another's subtree comes later); raises on a dependency cycle."""

    def depends_on(consumer_edge, provider_edge):
        source = consumer_edge[1]
        provided = provider_edge[0]
        return source == provided or source.startswith(provided + ".")

    ordered = []
    state = {}  # index -> "visiting" | "done"

    def visit(index):
        if state.get(index) == "done":
            return
        if state.get(index) == "visiting":
            raise SchemaError(f"inheritFrom cycle involving '{edges[index][0]}'")
        state[index] = "visiting"
        for other, edge in enumerate(edges):
            if other != index and depends_on(edges[index], edge):
                visit(other)
        state[index] = "done"
        ordered.append(edges[index])

    for index in range(len(edges)):
        visit(index)
    return ordered


def gen_resolve(module):
    """Emit the hidden-friend `resolve(out, overrides, key)`: resets `out` to defaults, overlays the accumulated overrides, then fills each leaf that was not explicitly set from its inherited source."""
    edges = collect_inherit_edges(module)
    out = [
        "\t\t/**",
        "\t\t * Resolves the accumulated overrides into a fully-concrete config: resets `out` to the defaults, overlays the overrides, then fills every leaf that was not explicitly set from its inherited source.",
        "\t\t */",
        f"\t\tfriend void resolve(Root& out, const Root& overrides, {ACCESS_TYPE} key) {{",
        "\t\t\tout = Root{};",
        "\t\t\toverlay(out, overrides, key);",
    ]
    # Edges are topologically ordered, so every source is fully resolved before a consumer reads it.
    for consumer, source, type_module, type_cfg in edges:
        for leaf in walk_leaves(type_module, type_cfg):
            out.append(f"\t\t\tif (!overrides.{consumer}.{leaf}.assigned()) {{")
            out.append(f"\t\t\t\tout.{consumer}.{leaf} = out.{source}.{leaf}.value();")
            out.append("\t\t\t}")
    out.append("\t\t}")
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


def gen_default(cfg, module):
    """Emit `inline const Root Default` (every field at its schema default) for the `Root` of a `defaultInstance` schema, so consumers can fall back to it; empty otherwise."""
    if not module.default_instance or cfg["name"] != "Root":
        return ""
    return "\n".join([
        "\t/**",
        "\t * Built-in defaults (every field at its schema default); the fallback when a config value fails to parse.",
        "\t */",
        "\tinline const Root Default{};",
    ])


def module_has_string(module):
    """Whether any leaf field resolves to `::std::string` (a plain string, or a `kind` color / length / easing)."""
    return any(not is_nested(field) and leaf_type(field) == "::std::string" for cfg in module.items for field in cfg["fields"])


def collect_all_enums(module, seen=None, out=None):
    """Every `(namespace, enum_name, values)` across a module and its transitive imports, de-duplicated by schema path."""
    seen = seen if seen is not None else set()
    out = out if out is not None else []
    key = module.path.resolve()
    if key in seen:
        return out
    seen.add(key)
    for name, (values, _comment) in collect_enums(module).items():
        out.append((module.namespace, name, values))
    for imported in module.imports.values():
        collect_all_enums(imported, seen, out)
    return out


def glaze_file_name(header_file):
    """The Glaze meta header paired with a schema's generated header `file` (`x.gen.h` -> `x.gen.glaze.h`)."""
    if header_file.endswith(".gen.h"):
        return header_file[: -len(".gen.h")] + ".gen.glaze.h"
    return Path(header_file).stem + ".gen.glaze.h"


def render_glaze(module, schema_name):
    """Render the Glaze enum-name meta header for a root config, or None when its whole tree declares no enums.

    Glaze serializes enums as integers on C++23 (name reflection needs C++26 P2996). A `glz::meta` per
    enum names its enumerators so the JSON wire carries the names; one aggregate header covers the tree.
    """
    enums = collect_all_enums(module)
    if not enums:
        return None
    guard = module.namespace.replace("::", "_").upper() + "_CONFIG_GLAZE_H_"
    parts = [
        f"// Generated by script/generate-config.py from {schema_name}. DO NOT EDIT.",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "glaze/json.hpp"',
        f'#include "{include_path_for(module.path.parent / module.file)}"',
    ]
    for namespace, name, values in enums:
        full = f"::{namespace}::{name}"
        parts += [
            "",
            "template <>",
            f"struct glz::meta<{full}> {{",
            f"\tusing enum {full};",
            f"\tstatic constexpr auto value = enumerate({', '.join(values)});",
            "};",
        ]
    parts += ["", f"#endif // {guard}"]
    return "\n".join(parts) + "\n"


def render(module, schema_name):
    """Render the whole generated header as a string."""
    namespace = module.namespace
    items = module.items
    guard = namespace.replace("::", "_").upper() + "_CONFIG_GEN_H_"
    parts = [
        "// clang-format off",
        "",
        f"// Generated by script/generate-config.py from {schema_name}. DO NOT EDIT.",
        "// To change config, edit the schema and regenerate with script/generate-config.py.",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
    ]

    stdlib = []
    if module_has_string(module):
        stdlib.append("#include <string>")  # string leaves become `Property<::std::string>`
    if stdlib:
        parts += ["", *stdlib]
    referenced = [f'#include "{path}"' for path in referenced_includes(module)]
    if referenced:
        parts += ["", *referenced]
    parts += ["", f'#include "{ACCESS_INCLUDE}"', f'#include "{PROPERTY_INCLUDE}"']  # the passkey and the leaf wrapper every config uses

    parts += ["", f"namespace {namespace} {{"]

    # enums first (referenced by the structs below); then the value structs, each leaf a `Property<T>` (value plus presence) and each carrying `operator==` plus overlay / capture (and the root's resolve) as machinery; then the root Default instance (only when the schema opts in).
    for block in gen_enums(module):
        parts.append(block)
        parts.append("")
    for cfg in items:
        parts.append(gen_struct(cfg, module))
        parts.append("")
    for cfg in items:
        block = gen_default(cfg, module)
        if block:
            parts.append(block)
            parts.append("")

    parts.append(f"}} // namespace {namespace}")
    parts.append("")
    parts.append(f"#endif // {guard}")
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

    # Root configs also get an aggregate Glaze enum-name meta header (skipped when the tree has no enums).
    if module.default_instance:
        glaze = render_glaze(module, schema_path.name)
        if glaze is not None:
            glaze_path = schema_path.parent / glaze_file_name(module.file)
            glaze_path.write_text(glaze, encoding="utf-8")
            print(f"generated {glaze_path} from {schema_path}")
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
