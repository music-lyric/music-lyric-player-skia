import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCES = (os.path.join(ROOT, "src"),)

EXCLUDES = {}

# Generated files carry their own `// clang-format off`; skip them here too.
EXCLUDE_SUFFIXES = (".gen.h",)

EXTENSIONS = (".cpp", ".h")


def handle(path: str, formater: str):
    files = []
    for dirpath, _, filenames in os.walk(path):
        for name in filenames:
            if name in EXCLUDES or name.endswith(EXCLUDE_SUFFIXES) or not name.endswith(EXTENSIONS):
                continue
            files.append(os.path.join(dirpath, name))
    files.sort()

    if not files:
        print(f"no sources found under {path}")
        return

    subprocess.run([formater, "-i", *files], check=True)
    print(f"formatted {len(files)} files under {path}")


def main():
    formater = shutil.which("clang-format")
    if formater is None:
        sys.exit("clang-format not found on PATH")

    for src in SOURCES:
        handle(src, formater)


if __name__ == "__main__":
    main()
