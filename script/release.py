#!/usr/bin/env python
"""
Bump the project version, regenerate the change log, then commit and tag the release locally.

The version lives in VERSION.txt at the repo root and is the single source of truth shared with CMake.
Pass a semantic bump (major | minor | patch) or an explicit --version X.Y.Z, or run with no argument to enter it interactively.
The commit and tag are created locally and never pushed.

Usage: release.py [major | minor | patch | --version X.Y.Z] [--dry-run]
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION_FILE = REPO_ROOT / "VERSION.txt"
CHANGE_LOG_BUILDER = REPO_ROOT / "script" / "change-log" / "build.py"

VERSION_REGEXP = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
PARTS = ("major", "minor", "patch")


def run_git(args, capture=True):
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        capture_output=capture,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        detail = result.stderr.strip() if capture else ""
        raise RuntimeError(f"failed: git {' '.join(args)}\n{detail}".strip())
    return result.stdout if capture else ""


def read_current_version():
    if not VERSION_FILE.exists():
        raise SystemExit(f"{VERSION_FILE.name} not found at repo root.")
    raw = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not VERSION_REGEXP.match(raw):
        raise SystemExit(f"invalid version in {VERSION_FILE.name}: {raw!r} (expected X.Y.Z)")
    return raw


def bump_version(current, part):
    major, minor, patch = (int(value) for value in current.split("."))
    if part == "major":
        return f"{major + 1}.0.0"
    if part == "minor":
        return f"{major}.{minor + 1}.0"
    return f"{major}.{minor}.{patch + 1}"


def prompt_version(current):
    """
    Accept an explicit X.Y.Z or a major/minor/patch keyword, repeating until valid.
    An empty line or interrupt aborts the release.
    """
    for part in PARTS:
        print(f"  {part:<5} -> {bump_version(current, part)}")
    while True:
        try:
            answer = input("new version (X.Y.Z or major/minor/patch): ").strip()
        except (EOFError, KeyboardInterrupt):
            raise SystemExit("\naborted.")
        if not answer:
            raise SystemExit("aborted.")
        if answer in PARTS:
            return bump_version(current, answer)
        if VERSION_REGEXP.match(answer):
            return answer
        print(f"invalid: {answer!r} (expected X.Y.Z or major/minor/patch)")


def confirm(prompt):
    try:
        answer = input(f"{prompt} [y/N]: ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        raise SystemExit("\naborted.")
    return answer in ("y", "yes")


def resolve_target(args, current):
    """
    Prefer an explicit --version, then a semantic bump, then an interactive prompt.
    """
    if args.version:
        if not VERSION_REGEXP.match(args.version):
            raise SystemExit(f"invalid --version: {args.version!r} (expected X.Y.Z)")
        return args.version
    if args.part:
        return bump_version(current, args.part)
    return prompt_version(current)


def ensure_clean_worktree():
    if run_git(["status", "--porcelain"]).strip():
        raise SystemExit("working tree is not clean; commit or stash changes first.")


def ensure_tag_absent(tag):
    if run_git(["tag", "--list", tag]).strip():
        raise SystemExit(f"tag {tag} already exists.")


def write_version(version):
    VERSION_FILE.write_text(f"{version}\n", encoding="utf-8", newline="\n")


def build_change_log():
    subprocess.run([sys.executable, str(CHANGE_LOG_BUILDER)], cwd=REPO_ROOT, check=True)


def parse_args():
    parser = argparse.ArgumentParser(description="Bump version, build change log, then commit and tag a release.")
    parser.add_argument("part", nargs="?", choices=PARTS, help="semantic version part to bump")
    parser.add_argument("--version", metavar="X.Y.Z", help="use an explicit version instead of bumping")
    parser.add_argument("--dry-run", action="store_true", help="print the planned release without changing anything")
    args = parser.parse_args()
    if args.part and args.version:
        parser.error("provide either major|minor|patch or --version X.Y.Z, not both")
    return args


def main():
    """
    Resolve the target version, confirm when interactive, then write, build, commit and tag.
    """
    args = parse_args()
    current = read_current_version()
    print(f"current version : {current}")
    interactive = not args.part and not args.version
    target = resolve_target(args, current)
    if target == current:
        raise SystemExit(f"target version equals current version ({current}); nothing to do.")
    tag = f"v{target}"

    if args.dry_run:
        print(f"[dry-run] version : {current} -> {target}")
        print(f"[dry-run] commit  : release: {tag}")
        print(f"[dry-run] tag     : {tag} (annotated, not pushed)")
        print("[dry-run] files   : VERSION.txt, CHANGELOG.md")
        return

    if interactive and not confirm(f"release {tag}?"):
        raise SystemExit("aborted.")

    ensure_clean_worktree()
    ensure_tag_absent(tag)

    write_version(target)
    build_change_log()

    run_git(["add", "VERSION.txt", "CHANGELOG.md"], capture=False)
    run_git(["commit", "-m", f"release: {tag}"], capture=False)
    run_git(["tag", "-a", tag, "-m", f"release {tag}"], capture=False)

    print(f"released {tag} ({current} -> {target}).")
    print("commit and tag created locally; push with: git push --follow-tags")


if __name__ == "__main__":
    main()
