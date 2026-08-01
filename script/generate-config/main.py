"""
Generate C++ config headers from JSON schema.

Reads each `*.schema.json` (all of them under the project when no path is given) and writes its `file` header alongside: one config struct per node, plus a Glaze enum-name meta header for `defaultInstance` roots and the TypeScript declarations a `typescript` block asks for and the Kotlin types a `kotlin` block asks for.
The schema shape is documented in the `*.schema.json` files themselves.

Usage: `python script/generate-config/main.py [<schema.json> ...]`.
"""

import os
import sys

from model import SKIP_DIRS, SchemaError
from schema import load_module
from emit import render
from glaze import glaze_file_name, render_glaze
from typescript import OUT_DIR as TYPESCRIPT_OUT_DIR, render_typescript
from kotlin import OUT_DIR as KOTLIN_OUT_DIR, render_kotlin


def scan_schemas(root):
    """Find every *.schema.json under root, pruning build / vendored directories."""
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.endswith(".schema.json"):
                found.append(os.path.join(dirpath, name))
    return sorted(found)


def generate_one(schema_path):
    """Load, validate and generate a single schema; 0 on success, 1 on error."""
    try:
        module = load_module(schema_path, cache={})
    except SchemaError as error:
        print(f"error: {schema_path}: {error}", file=sys.stderr)
        return 1

    schema_name = os.path.basename(schema_path)
    out_path = os.path.join(os.path.dirname(schema_path), module.file)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as handle:
        handle.write(render(module, schema_name))
    print(f"generated {out_path} from {schema_path}")

    # defaultInstance roots also get an aggregate Glaze enum-name meta header.
    if module.default_instance:
        glaze = render_glaze(module, schema_name)
        if glaze is not None:
            glaze_path = os.path.join(os.path.dirname(schema_path), glaze_file_name(module.file))
            with open(glaze_path, "w", encoding="utf-8") as handle:
                handle.write(glaze)
            print(f"generated {glaze_path} from {schema_path}")

    # A schema opts into TypeScript through its `typescript` block, which lands in the web package rather than beside the schema.
    if module.typescript.enable:
        typescript = render_typescript(module, schema_name)
        if typescript is not None:
            typescript_path = os.path.join(TYPESCRIPT_OUT_DIR, module.typescript.out)
            os.makedirs(os.path.dirname(typescript_path), exist_ok=True)
            with open(typescript_path, "w", encoding="utf-8") as handle:
                handle.write(typescript)
            print(f"generated {typescript_path} from {schema_path}")

    # A schema opts into Kotlin through its `kotlin` block, which lands in the Gradle module's source set under the package it declares.
    if module.kotlin.enable:
        kotlin = render_kotlin(module, schema_name)
        if kotlin is not None:
            kotlin_path = os.path.join(KOTLIN_OUT_DIR, *module.kotlin.package.split("."), module.kotlin.out)
            os.makedirs(os.path.dirname(kotlin_path), exist_ok=True)
            with open(kotlin_path, "w", encoding="utf-8") as handle:
                handle.write(kotlin)
            print(f"generated {kotlin_path} from {schema_path}")
    return 0


def main(argv):
    given = argv[1:]
    if given:
        schemas = [os.path.normpath(path) for path in given]
        missing = [path for path in schemas if not os.path.isfile(path)]
        if missing:
            print("error: schema not found: " + ", ".join(missing), file=sys.stderr)
            return 1
    else:
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
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
