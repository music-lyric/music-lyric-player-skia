"""
Naming and traversal shared by the language-binding emitters: the flat type names one config tree exposes, the nodes it reaches and the enums it declares.
Every binding reads these, so a name the TypeScript declarations use and the Kotlin types use cannot drift apart.
"""

from model import SchemaError, is_nested, resolve_nested
from glaze import collect_all_enums


def pascal_case(segment):
    """Render a namespace segment in PascalCase."""
    return "".join(part[:1].upper() + part[1:] for part in segment.split("_") if part)


def namespace_tail(root_namespace, namespace):
    """The namespace segments that separate a module from the root, which is what keeps the flat binding names apart."""
    root_parts = root_namespace.split("::")
    parts = namespace.split("::")
    index = 0
    while index < len(root_parts) and index < len(parts) and root_parts[index] == parts[index]:
        index += 1
    return parts[index:]


def type_name(root_namespace, namespace, name, root_name):
    """
    Flat binding name for a node or enum, built from its namespace tail so the result mirrors the C++ path and never depends on what else the tree happens to contain.
    A nested `Root` reads as `Config` because `LineNormalRoot` would name the same thing far less clearly; the tree's own `Root` takes the name the schema chose.
    """
    prefix = "".join(pascal_case(segment) for segment in namespace_tail(root_namespace, namespace))
    if name == "Root":
        return f"{prefix}Config" if prefix else root_name
    return f"{prefix}{name}"


class Naming:
    """Maps the C++ names of one tree onto the flat binding names, which every emitter shares."""

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
    This reads the same source as the Glaze meta header, so the names a binding spells and the names on the wire cannot drift apart.
    """
    return {(namespace, name): values for namespace, name, values in collect_all_enums(root_module)}


def check_unique(language, names):
    """Reject two declarations claiming one binding name, which would otherwise emit a file that does not compile."""
    seen = {}
    for origin, name in names:
        if name in seen:
            raise SchemaError(f"'{origin}' and '{seen[name]}' both map to the {language} name '{name}'")
        seen[name] = origin
