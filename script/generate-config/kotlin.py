"""
Emits the Kotlin types of a root config tree: one data class per node with every field nullable, which is the shape of the sparse JSON patch that crosses the ABI.
Serialization runs one way only, because the app writes a config and the renderer reads one, and a reader would cost the consumer a serialization runtime it has no other use for.
"""

import os

from model import KIND_NOTES, SchemaError, is_nested, leaf_type, resolve_nested
from binding import Naming, check_unique, collect_nodes, enum_index
from emit import doc_lines, jsdoc

INDENT = "    "

# Source set of the Gradle module: the types are part of what the aar delivers, so they sit with the hand-written Kotlin rather than beside the schema.
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "platform",
    "android",
    "src",
    "main",
    "kotlin",
)

# C++ leaf types mapped onto Kotlin; a `kind` leaf is a ::std::string by definition and an enum resolves to its own class.
TYPE_MAP = {
    "bool": "Boolean",
    "int": "Int",
    "float": "Float",
    "double": "Double",
    "::std::string": "String",
}

# Kotlin's hard keywords, which a schema is free to use as a field name and which only backticks let through.
KEYWORDS = {
    "as",
    "break",
    "class",
    "continue",
    "do",
    "else",
    "false",
    "for",
    "fun",
    "if",
    "in",
    "interface",
    "is",
    "null",
    "object",
    "package",
    "return",
    "super",
    "this",
    "throw",
    "true",
    "try",
    "typealias",
    "typeof",
    "val",
    "var",
    "when",
    "while",
}


def escape(name):
    """The name as a Kotlin identifier, backticked when the language reserves it, so a schema never has to dodge a word the wire already uses."""
    return f"`{name}`" if name in KEYWORDS else name


def field_type(naming, module, field, enums):
    """Kotlin type of a field: the referenced data class for a nested config, the enum class for an enum, else the mapped scalar."""
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
        raise SchemaError(f"leaf '{field['name']}' is '{cpp}', which has no Kotlin mapping")
    return mapped


def default_tag(field):
    """
    The @default value as Kotlin.
    An enum default is a bare enumerator that has to be quoted into its wire string; every other C++ literal already reads as Kotlin.
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
    """Block KDoc above a field: its comment lines, the kind format note, then the auto tags."""
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
    """Emit an enum whose entries carry the name Glaze puts on the wire, so the JSON never depends on what the Kotlin entry happens to be called."""
    lines = [f"enum class {naming.of(namespace, name)}(val wire: String) {{"]
    lines += [f'{INDENT}{escape(value)}("{value}"),' for value in values]
    lines.append("}")
    return "\n".join(lines)


def json_put(field, name):
    """The expression that writes one set field into the JSON object, unwrapping a nested node and an enum into what the wire carries."""
    if is_nested(field):
        return f'json.put("{name}", it.toJson())'
    if "enum" in field:
        return f'json.put("{name}", it.wire)'
    return f'json.put("{name}", it)'


def gen_to_json(cfg):
    """Emit the one-way serializer of a node, which puts only the fields that were set and so keeps the patch sparse."""
    lines = [
        f"{INDENT}/**",
        f"{INDENT} * Renders the fields that are set into a JSON object, leaving the rest out so the result stays a sparse patch.",
        f"{INDENT} */",
        f"{INDENT}fun toJson(): JSONObject {{",
        f"{INDENT * 2}val json = JSONObject()",
    ]
    for field in cfg["fields"]:
        name = field["name"]
        lines.append(f"{INDENT * 2}this.{escape(name)}?.let {{ {json_put(field, name)} }}")
    lines += [f"{INDENT * 2}return json", f"{INDENT}}}"]
    return "\n".join(lines)


def gen_class(naming, module, cfg, enums):
    """Emit one node as a data class whose fields are all nullable, because a patch carries only what it means to change."""
    out = [jsdoc(cfg.get("comment"), "") + f"data class {naming.of(module.namespace, cfg['name'])}("]
    for field in cfg["fields"]:
        declaration = f"{escape(field['name'])}: {field_type(naming, module, field, enums)}? = null"
        out.append(f"{field_doc(field)}{INDENT}val {declaration},")
    out += [") {", gen_to_json(cfg), "}"]
    return "\n".join(out)


def render_kotlin(module, schema_name):
    """Render the Kotlin types of a root config tree, or None when the schema declares no `Root`."""
    if "Root" not in module.by_name:
        return None
    naming = Naming(module.namespace, module.kotlin.root)
    enums = enum_index(module)
    nodes = collect_nodes(module)

    check_unique(
        "Kotlin",
        [(f"{namespace}::{name}", naming.of(namespace, name)) for namespace, name in enums]
        + [
            (f"{node_module.namespace}::{cfg['name']}", naming.of(node_module.namespace, cfg["name"]))
            for node_module, cfg in nodes
        ],
    )

    parts = [
        f"// Generated by generate-config script from {schema_name}. DO NOT EDIT.",
        "",
        f"package {module.kotlin.package}",
        "",
        "import org.json.JSONObject",
        "",
    ]
    # Enums first, the way the generated header orders them, since the classes below reference them.
    for (namespace, name), values in enums.items():
        parts += [gen_enum(naming, namespace, name, values), ""]
    for node_module, cfg in nodes:
        parts += [gen_class(naming, node_module, cfg, enums), ""]
    return "\n".join(parts)
