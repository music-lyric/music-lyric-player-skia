"""Schema normalisation (mix / root expansion), structural validation and import resolution."""

import copy
import json

from model import KIND_NOTES, Module, SchemaError, is_nested, is_valid_comment


def expand_mix(schema):
    """Expand `{ "include": ... }` field entries from schema-local `mix`, applying any leaf-default overrides."""
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
                if (
                    not isinstance(defaults, dict)
                    or ("defaults" in field and not defaults)
                    or not all(isinstance(k, str) and isinstance(v, str) for k, v in defaults.items())
                ):
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
    """Expand a top-level `root` (an ordered module list) into imports plus a generated `Root`."""
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
    """Reject a malformed schema; dotted `nested` targets are checked in `validate_refs`."""
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
                    raise SchemaError(
                        f"{at} ('{field_name}') is nested and must not have 'type'/'kind'/'enum'/'values'/'default'/'example'/'min'/'max'"
                    )
                overrides = field.get("defaults")
                if overrides is not None and not (
                    isinstance(overrides, dict)
                    and overrides
                    and all(isinstance(k, str) and isinstance(v, str) for k, v in overrides.items())
                ):
                    raise SchemaError(
                        f"{at} ('{field_name}').defaults must be a non-empty object of field -> C++ literal string"
                    )
                if "inheritFrom" in field and (not isinstance(field["inheritFrom"], str) or not field["inheritFrom"]):
                    raise SchemaError(
                        f"{at} ('{field_name}').inheritFrom must be a non-empty string (a dotted path from Root to the inherited source)"
                    )
                target = field["nested"]
                if not isinstance(target, str) or not target:
                    raise SchemaError(f"{at} ('{field_name}').nested must be a non-empty string")
                if "." in target:
                    alias = target.split(".", 1)[0]
                    if alias not in aliases:
                        raise SchemaError(
                            f"{at} ('{field_name}') nests '{target}', but import '{alias}' is not declared"
                        )
                elif target not in defined:
                    raise SchemaError(f"{at} ('{field_name}') nests '{target}', which must be defined before '{name}'")
            else:
                enum_name = field.get("enum")
                kind = field.get("kind")
                if enum_name is not None:
                    if not isinstance(enum_name, str) or not enum_name:
                        raise SchemaError(
                            f"{at} ('{field_name}').enum must be a non-empty string (the enum class name)"
                        )
                    if "type" in field or kind is not None:
                        raise SchemaError(f"{at} ('{field_name}') has 'enum' and must not also set 'type'/'kind'")
                    values = field.get("values")
                    if not (isinstance(values, list) and values and all(isinstance(v, str) and v for v in values)):
                        raise SchemaError(
                            f"{at} ('{field_name}').values must be a non-empty array of enumerator name strings"
                        )
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
                        raise SchemaError(
                            f"{at} ('{field_name}') has 'kind' and must not also set 'type' (kind implies ::std::string)"
                        )
                else:
                    field_type = field.get("type")
                    if not isinstance(field_type, str) or not field_type:
                        raise SchemaError(
                            f"{at} ('{field_name}') must have a non-empty 'type', a 'kind', an 'enum', or 'nested'"
                        )
                if "inheritFrom" in field:
                    raise SchemaError(
                        f"{at} ('{field_name}') has 'inheritFrom' but is a leaf; inheritance is only supported on nested fields"
                    )
                if "default" in field and not isinstance(field["default"], str):
                    raise SchemaError(f"{at} ('{field_name}').default must be a string holding a C++ literal")

        defined.add(name)


def validate_refs(module):
    """Verify every dotted `nested` target resolves to a struct in the named import."""
    for cfg in module.items:
        for field in cfg["fields"]:
            if not is_nested(field) or "." not in field["nested"]:
                continue
            alias, _, struct = field["nested"].partition(".")
            target = module.imports.get(alias)
            if target is None:
                raise SchemaError(
                    f"config '{cfg['name']}' nests '{field['nested']}', but import '{alias}' failed to load"
                )
            if struct not in target.by_name:
                raise SchemaError(
                    f"config '{cfg['name']}' nests '{field['nested']}', but '{struct}' is not defined in that schema"
                )


def load_module(path, cache, loading=None):
    """Read, validate and resolve a schema and its imports into a Module, memoised by path."""
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
