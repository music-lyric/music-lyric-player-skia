"""C++ header emission: enum blocks, the unified config struct with its hidden overlay / capture / resolve machinery, inheritance resolution and the full header body."""

from model import (
    ACCESS_INCLUDE,
    ACCESS_TYPE,
    KIND_NOTES,
    PROPERTY_INCLUDE,
    PROPERTY_TYPE,
    SchemaError,
    is_nested,
    leaf_default,
    leaf_type,
    referenced_includes,
    resolve_nested,
)


def doc_lines(desc):
    """Normalise a desc (None, a string, or a list) into a list of lines."""
    if desc is None:
        return []
    return list(desc) if isinstance(desc, list) else [desc]


def jsdoc(desc, indent):
    """Render a desc (string or list of lines) as a multi-line JSDoc block."""
    lines = doc_lines(desc)
    if not lines:
        return ""
    body = "\n".join(f"{indent} * {line}" if line else f"{indent} *" for line in lines)
    return f"{indent}/**\n{body}\n{indent} */\n"


def field_tags(field):
    """The @default / @example / @minimum / @maximum JSDoc lines from a field's metadata."""

    def render(value):
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, str):
            return f'"{value}"'
        return str(value)

    lines = []
    if "default" in field:
        default = f"{field['enum']}::{field['default']}" if "enum" in field else field["default"]
        lines.append(f"@default {default}")
    example = field.get("example")
    for item in example if isinstance(example, list) else ([] if example is None else [example]):
        lines.append(f"@example {render(item)}")
    if "min" in field:
        lines.append(f"@minimum {render(field['min'])}")
    if "max" in field:
        lines.append(f"@maximum {render(field['max'])}")
    return lines


def field_doc(field, indent, inline):
    """Block JSDoc above a field: its comment lines, the kind format note, then the auto tags."""
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
    """Trailing inline `// comment` for an inline-module string comment, else empty."""
    comment = field.get("comment")
    return f"  // {comment}" if inline and isinstance(comment, str) and comment else ""


def gen_struct(cfg, module):
    """Emit the struct; leaves carry their defaults and the machinery rides along as hidden friends."""
    out = [jsdoc(cfg.get("comment"), "\t") + f"\tstruct {cfg['name']} {{"]
    for field in cfg["fields"]:
        above = field_doc(field, "\t\t", module.inline)
        if is_nested(field):
            _, _, type_str = resolve_nested(module, field["nested"])
            overrides = field.get("defaults")
            if overrides:
                # C++20 designated initialisers; unlisted sub-fields keep their in-class defaults.
                inits = ", ".join(f".{key} = {value}" for key, value in overrides.items())
                out.append(
                    f"{above}\t\t{type_str} {field['name']} = {type_str}{{ {inits} }};{note(field, module.inline)}"
                )
            else:
                out.append(f"{above}\t\t{type_str} {field['name']};{note(field, module.inline)}")
        else:
            out.append(
                f"{above}\t\t{PROPERTY_TYPE}<{leaf_type(field)}> {field['name']} = {leaf_default(field)};{note(field, module.inline)}"
            )
    # operator== lets Manager detect real changes; overlay / capture / resolve ride as passkey-gated hidden friends found only via ADL.
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
    """Emit the hidden-friend `overlay`: copy every explicitly-set leaf from `src` onto `dst`."""
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
    """Emit the hidden-friend `capture`: record leaves that changed between `prev` and `next` into `delta`."""
    name = cfg["name"]
    fields = cfg["fields"]
    obj = "" if fields else "[[maybe_unused]] "
    key = "" if any(is_nested(field) for field in fields) else "[[maybe_unused]] "
    out = [
        f"\t\tfriend void capture({obj}{name}& delta, {obj}const {name}& prev, {obj}const {name}& next, {key}{ACCESS_TYPE} key) {{"
    ]
    for field in fields:
        n = field["name"]
        if is_nested(field):
            out.append(f"\t\t\tcapture(delta.{n}, prev.{n}, next.{n}, key);")
        else:
            out.append(f"\t\t\tif (!(prev.{n} == next.{n})) delta.{n} = next.{n}.value();")
    out.append("\t\t}")
    return "\n".join(out)


def walk_leaves(module, cfg, prefix=""):
    """Every leaf's dotted sub-path under a config, recursing through nested fields."""
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
    """Resolve a dotted absolute path from `Root` to the (module, cfg) it names; raises on a missing or leaf segment."""
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
    """Collect every `inheritFrom` edge under `Root` as (consumer, source, type_module, type_cfg), validated and ordered; rejects a self / descendant / type-mismatched source."""
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
                    raise SchemaError(
                        f"inheritFrom '{source}' resolves to '{source_cfg['name']}' but field '{path}' is '{target_cfg['name']}'; the inherited source must be the same type"
                    )
                edges.append((path, source, target_module, target_cfg))
            walk(target_module, target_cfg, path + ".")

    walk(root_module, root_module.by_name["Root"], "")
    return order_inherit_edges(edges)


def order_inherit_edges(edges):
    """Order fix-ups so every source is resolved before it is read; raises on a dependency cycle."""

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
    """Emit the hidden-friend `resolve`: reset `out` to defaults, overlay overrides, then fill unset leaves from their inherited source."""
    edges = collect_inherit_edges(module)
    out = [
        "\t\t/**",
        "\t\t * Resolves the accumulated overrides into a fully-concrete config: resets `out` to the defaults, overlays the overrides, then fills every leaf that was not explicitly set from its inherited source.",
        "\t\t */",
        f"\t\tfriend void resolve(Root& out, const Root& overrides, {ACCESS_TYPE} key) {{",
        "\t\t\tout = Root{};",
        "\t\t\toverlay(out, overrides, key);",
    ]
    # Topologically ordered, so every source is resolved before a consumer reads it.
    for consumer, source, type_module, type_cfg in edges:
        for leaf in walk_leaves(type_module, type_cfg):
            out.append(f"\t\t\tif (!overrides.{consumer}.{leaf}.assigned()) {{")
            out.append(f"\t\t\t\tout.{consumer}.{leaf} = out.{source}.{leaf}.value();")
            out.append("\t\t\t}")
    out.append("\t\t}")
    return "\n".join(out)


def collect_enums(module):
    """Ordered {enum_name: (values, comment)} across the module's leaf fields; errors on a conflicting redefinition."""
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
    """The `enum class` blocks for the module's enum fields."""
    blocks = []
    for name, (values, comment) in collect_enums(module).items():
        lines = [jsdoc(comment, "\t") + f"\tenum class {name} {{"]
        lines += [f"\t\t{value}," for value in values]
        lines.append("\t};")
        blocks.append("\n".join(lines))
    return blocks


def gen_default(cfg, module):
    """Emit `inline const Root Default` for a `defaultInstance` schema's `Root`, else empty."""
    if not module.default_instance or cfg["name"] != "Root":
        return ""
    return "\n".join(
        [
            "\t/**",
            "\t * Built-in defaults (every field at its schema default); the fallback when a config value fails to parse.",
            "\t */",
            "\tinline const Root Default{};",
        ]
    )


def module_has_string(module):
    """Whether any leaf resolves to ::std::string."""
    return any(
        not is_nested(field) and leaf_type(field) == "::std::string" for cfg in module.items for field in cfg["fields"]
    )


def render(module, schema_name):
    """Render the whole generated header as a string."""
    namespace = module.namespace
    items = module.items
    guard = namespace.replace("::", "_").upper() + "_CONFIG_GEN_H_"
    parts = [
        "// clang-format off",
        "",
        "// Generated by generate-config script. DO NOT EDIT.",
        "// To change config, edit the schema and regenerate with generate-config script.",
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
    parts += ["", f'#include "{ACCESS_INCLUDE}"', f'#include "{PROPERTY_INCLUDE}"']

    parts += ["", f"namespace {namespace} {{"]

    # Enums first (the structs reference them), then the structs, then the opt-in Default instance.
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
