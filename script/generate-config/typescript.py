"""
Emits the TypeScript declarations of a root config tree: one interface per node with every field optional, which is the shape of the sparse JSON patch that crosses the ABI.
"""

import os

from model import KIND_NOTES, SchemaError, is_nested, leaf_type, resolve_nested
from emit import doc_lines, jsdoc
from glaze import collect_all_enums

INDENT = "  "

# Delivery root of the web package: the declarations ship from there rather than from the source tree, since nothing in-tree consumes them.
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "platform", "web", "types"
)

# C++ leaf types mapped onto TypeScript; a `kind` leaf is a ::std::string by definition and an enum resolves to its own alias.
TYPE_MAP = {
    "bool": "boolean",
    "int": "number",
    "float": "number",
    "double": "number",
    "::std::string": "string",
}


def pascal_case(segment):
    """Render a namespace segment in PascalCase."""
    return "".join(part[:1].upper() + part[1:] for part in segment.split("_") if part)


def namespace_tail(root_namespace, namespace):
    """The namespace segments that separate a module from the root, which is what keeps the flat TypeScript names apart."""
    root_parts = root_namespace.split("::")
    parts = namespace.split("::")
    index = 0
    while index < len(root_parts) and index < len(parts) and root_parts[index] == parts[index]:
        index += 1
    return parts[index:]


def type_name(root_namespace, namespace, name, root_name):
    """
    Flat TypeScript name for a node or enum, built from its namespace tail so the result mirrors the C++ path and never depends on what else the tree happens to contain.
    A nested `Root` reads as `Config` because `LineNormalRoot` would name the same thing far less clearly; the tree's own `Root` takes the name the schema chose.
    """
    prefix = "".join(pascal_case(segment) for segment in namespace_tail(root_namespace, namespace))
    if name == "Root":
        return f"{prefix}Config" if prefix else root_name
    return f"{prefix}{name}"


class Naming:
    """Maps the C++ names of one tree onto the flat TypeScript names, which every emitter below shares."""

    def __init__(self, root_namespace, root_name):
        self.root_namespace = root_namespace
        self.root_name = root_name

    def of(self, namespace, name):
        return type_name(self.root_namespace, namespace, name, self.root_name)


def collect_nodes(root_module):
    """Every (module, cfg) reachable from `Root` in reading order, de-duplicated so a shared node is declared once and referenced everywhere."""
    nodes = []
    seen = set()

    def walk(module, cfg):
        key = (module.namespace, cfg["name"])
        if key in seen:
            return
        seen.add(key)
        nodes.append((module, cfg))
        for field in cfg["fields"]:
            if is_nested(field):
                target_module, target_cfg, _ = resolve_nested(module, field["nested"])
                walk(target_module, target_cfg)

    walk(root_module, root_module.by_name["Root"])
    return nodes


def enum_index(root_module):
    """
    {(namespace, enum): values} for the whole tree.
    This reads the same source as the Glaze meta header, so the unions here and the names on the wire cannot drift apart.
    """
    return {(namespace, name): values for namespace, name, values in collect_all_enums(root_module)}


def check_unique(names):
    """Reject two declarations claiming one TypeScript name, which would otherwise emit a file that does not compile."""
    seen = {}
    for origin, name in names:
        if name in seen:
            raise SchemaError(f"'{origin}' and '{seen[name]}' both map to the TypeScript name '{name}'")
        seen[name] = origin


def field_type(naming, module, field, enums):
    """TypeScript type of a field: the referenced interface for a nested config, the enum alias for an enum, else the mapped scalar."""
    if is_nested(field):
        target_module, target_cfg, _ = resolve_nested(module, field["nested"])
        return naming.of(target_module.namespace, target_cfg["name"])
    if "enum" in field:
        key = (module.namespace, field["enum"])
        if key not in enums:
            raise SchemaError(f"leaf '{field['name']}' names enum '{field['enum']}', which the tree does not declare")
        return naming.of(module.namespace, field["enum"])
    cpp = leaf_type(field)
    mapped = TYPE_MAP.get(cpp)
    if mapped is None:
        raise SchemaError(f"leaf '{field['name']}' is '{cpp}', which has no TypeScript mapping")
    return mapped


def default_tag(field):
    """
    The @default value as TypeScript.
    An enum default is a bare enumerator that has to be quoted into its wire string; every other C++ literal already reads as TypeScript.
    """
    if "default" not in field:
        return None
    if "enum" in field:
        return f'"{field["default"]}"'
    return field["default"]


def field_tags(field):
    """The @default / @example / @minimum / @maximum / @see lines from a field's metadata."""

    def render(value):
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, str):
            return f'"{value}"'
        return str(value)

    lines = []
    default = default_tag(field)
    if default is not None:
        lines.append(f"@default {default}")
    example = field.get("example")
    for item in example if isinstance(example, list) else ([] if example is None else [example]):
        lines.append(f"@example {render(item)}")
    if "min" in field:
        lines.append(f"@minimum {render(field['min'])}")
    if "max" in field:
        lines.append(f"@maximum {render(field['max'])}")
    # The schema prose says a field inherits; only the path itself is worth restating, so it rides as a tag.
    if "inheritFrom" in field:
        lines.append(f"@see {field['inheritFrom']}")
    return lines


def field_doc(field):
    """Block JSDoc above a field: its comment lines, the kind format note, then the auto tags."""
    lines = doc_lines(field.get("comment"))
    kind = field.get("kind")
    if kind:
        lines.append(KIND_NOTES[kind])
    tags = field_tags(field)
    if tags:
        if lines and lines[-1] != "":
            lines.append("")
        lines += tags
    return jsdoc(lines, INDENT)


def gen_enum(naming, namespace, name, values):
    """Emit an enum as a string union, matching the enumerator names Glaze puts on the wire."""
    union = " | ".join(f'"{value}"' for value in values)
    return f"export type {naming.of(namespace, name)} = {union};"


def gen_interface(naming, module, cfg, enums):
    """Emit one node as an interface whose fields are all optional, because a patch carries only what it means to change."""
    out = [jsdoc(cfg.get("comment"), "") + f"export interface {naming.of(module.namespace, cfg['name'])} {{"]
    for field in cfg["fields"]:
        out.append(f"{field_doc(field)}{INDENT}{field['name']}?: {field_type(naming, module, field, enums)};")
    out.append("}")
    return "\n".join(out)


def render_typescript(module, schema_name):
    """Render the TypeScript declarations of a root config tree, or None when the schema declares no `Root`."""
    if "Root" not in module.by_name:
        return None
    naming = Naming(module.namespace, module.typescript.root)
    enums = enum_index(module)
    nodes = collect_nodes(module)

    check_unique(
        [(f"{namespace}::{name}", naming.of(namespace, name)) for namespace, name in enums]
        + [
            (f"{node_module.namespace}::{cfg['name']}", naming.of(node_module.namespace, cfg["name"]))
            for node_module, cfg in nodes
        ]
    )

    parts = [f"// Generated by generate-config script from {schema_name}. DO NOT EDIT.", ""]
    # Enums first, the way the generated header orders them, since the interfaces below reference the aliases.
    for (namespace, name), values in enums.items():
        parts += [gen_enum(naming, namespace, name, values), ""]
    for node_module, cfg in nodes:
        parts += [gen_interface(naming, node_module, cfg, enums), ""]
    return "\n".join(parts)
