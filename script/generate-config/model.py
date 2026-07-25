
SKIP_DIRS = {"build", "third-party", ".history", ".git", "node_modules", ".turbo", ".vscode", ".claude"}

# Format notes appended to a kind-typed leaf's doc.
KIND_NOTES = {
    "color": "Accepts `#RGB` / `#RGBA` / `#RRGGBB` / `#RRGGBBAA`, `rgb(r, g, b)` or `rgba(r, g, b, a)` (a in 0..1).",
    "length": "A bare number or `px` is an absolute length in logical pixels.",
    "easing": "Accepts `linear`, `ease`, `ease-in`, `ease-out`, `ease-in-out` or `cubic-bezier(x1, y1, x2, y2)`.",
}

# Passkey type gating the generated machinery; only Manager can mint one.
ACCESS_TYPE = "::music_lyric_player::utils::config::Access"

ACCESS_INCLUDE = "utils/config/access.h"

# Leaf wrapper: a value plus its presence flag.
PROPERTY_TYPE = "::music_lyric_player::utils::config::Property"

PROPERTY_INCLUDE = "utils/config/property.h"


class SchemaError(Exception):
    """A human-readable problem with the input schema."""


class Module:
    """A loaded schema plus its resolved imports."""

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
    return "nested" in field


def leaf_type(field):
    """C++ type of a leaf: its enum class, ::std::string for a kind, else the explicit type."""
    if "enum" in field:
        return field["enum"]
    return "::std::string" if "kind" in field else field["type"]


def leaf_default(field):
    """C++ default initialiser for a leaf (Enum::Value for enums, else the literal, {} when unset)."""
    if "enum" in field:
        return f"{field['enum']}::{field['default']}" if "default" in field else "{}"
    return field.get("default", "{}")


def is_valid_comment(value):
    return isinstance(value, str) or (isinstance(value, list) and all(isinstance(line, str) for line in value))


def resolve_nested(module, value):
    """Resolve a nested reference (bare local name or dotted alias.Struct) to (module, cfg, type)."""
    if "." in value:
        alias, _, struct = value.partition(".")
        target = module.imports[alias]
        type_str = f"::{target.namespace}::{struct}"
        return target, target.by_name[struct], type_str
    return module, module.by_name[value], value


def resolve_field_path(module, cfg, path):
    """Resolve a dotted field path under `cfg` into its owning module, field definition and C++ type."""
    parts = path.split(".")
    if not parts or any(not part for part in parts):
        raise SchemaError(f"invalid field path '{path}'")

    current_module, current_cfg = module, cfg
    for index, part in enumerate(parts):
        field = next((candidate for candidate in current_cfg["fields"] if candidate["name"] == part), None)
        if field is None:
            raise SchemaError(f"field path '{path}' has no field '{part}'")
        if index == len(parts) - 1:
            if is_nested(field):
                target_module, target_cfg, type_str = resolve_nested(current_module, field["nested"])
                return target_module, target_cfg, field, type_str
            return current_module, current_cfg, field, leaf_type(field)
        if not is_nested(field):
            raise SchemaError(f"field path '{path}' traverses leaf '{part}'")
        current_module, current_cfg, _ = resolve_nested(current_module, field["nested"])


def include_path_for(out_path):
    """Project-relative include path for a header, relative to the nearest src/."""
    parts = out_path.resolve().parts
    if "src" in parts:
        cut = len(parts) - 1 - parts[::-1].index("src")
        return "/".join(parts[cut + 1 :])
    return out_path.name


def referenced_includes(module):
    """Include paths for every imported header referenced by a nested field."""
    paths = set()
    for cfg in module.items:
        for field in cfg["fields"]:
            if is_nested(field) and "." in field["nested"]:
                target = module.imports[field["nested"].split(".", 1)[0]]
                paths.add(include_path_for(target.path.parent / target.file))
    return sorted(paths)
