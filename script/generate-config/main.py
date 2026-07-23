#!/usr/bin/env python3
"""Generate C++ config headers from JSON schema(s).

Reads each `*.schema.json` (all of them under the project when no path is given) and writes its `file` header alongside: one config struct per node plus a Glaze enum-name meta header for `defaultInstance` roots.
The schema shape is documented in the `*.schema.json` files themselves.

Usage: `python script/generate-config/main.py [<schema.json> ...]`.
"""

import os
import sys
from pathlib import Path

from model import SKIP_DIRS, SchemaError
from schema import load_module
from emit import render
from glaze import glaze_file_name, render_glaze


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
    """Load, validate and generate a single schema; 0 on success, 1 on error."""
    try:
        module = load_module(schema_path, cache={})
    except SchemaError as error:
        print(f"error: {schema_path}: {error}", file=sys.stderr)
        return 1

    out_path = schema_path.parent / module.file
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render(module, schema_path.name), encoding="utf-8")
    print(f"generated {out_path} from {schema_path}")

    # defaultInstance roots also get an aggregate Glaze enum-name meta header.
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
        root = Path(__file__).resolve().parent.parent.parent
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
