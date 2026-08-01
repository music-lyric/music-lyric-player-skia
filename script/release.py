"""
Bump the project version, regenerate the change log, then commit and tag the release locally.

Usage: release.py [major | minor | patch | --version X.Y.Z] [--dry-run]
"""

import argparse
import json
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_FILE = os.path.join(REPO_ROOT, "VERSION.txt")
CHANGE_LOG_FILE = os.path.join(REPO_ROOT, "CHANGELOG.md")
WEB_PACKAGE_FILE = os.path.join(REPO_ROOT, "platform", "web", "package.json")
ANDROID_PROPERTIES_FILE = os.path.join(REPO_ROOT, "platform", "android", "gradle.properties")
CHANGE_LOG_BUILDER = os.path.join(REPO_ROOT, "script", "change-log", "build.py")

VERSION_REGEXP = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
ANDROID_VERSION_REGEXP = re.compile(r"^VERSION_NAME=.*$", re.MULTILINE)
PARTS = ("major", "minor", "patch")

# Files the release rewrites, listed for the dry run and staged together in the release commit.
RELEASE_FILES = (VERSION_FILE, CHANGE_LOG_FILE, WEB_PACKAGE_FILE, ANDROID_PROPERTIES_FILE)


def repo_path(path):
    """Render a release file as a repo relative posix path, which is what git takes and the dry run prints."""
    return os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")


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
    if not os.path.exists(VERSION_FILE):
        raise SystemExit(f"{os.path.basename(VERSION_FILE)} not found at repo root.")
    with open(VERSION_FILE, encoding="utf-8") as handle:
        raw = handle.read().strip()
    if not VERSION_REGEXP.match(raw):
        raise SystemExit(f"invalid version in {os.path.basename(VERSION_FILE)}: {raw!r} (expected X.Y.Z)")
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
    """Prefer an explicit --version, then a semantic bump, then an interactive prompt."""
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
    with open(VERSION_FILE, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{version}\n")


def write_web_package_version(version):
    """
    Rewrite the version field of the web npm manifest so the published package tracks VERSION.txt.
    Parsing as JSON both validates the manifest and keeps the release from tagging a package npm would reject.
    """
    if not os.path.exists(WEB_PACKAGE_FILE):
        raise SystemExit(f"{repo_path(WEB_PACKAGE_FILE)} not found.")
    try:
        with open(WEB_PACKAGE_FILE, encoding="utf-8") as handle:
            manifest = json.loads(handle.read())
    except json.JSONDecodeError as error:
        raise SystemExit(f"invalid JSON in {repo_path(WEB_PACKAGE_FILE)}: {error}")
    manifest["version"] = version
    serialized = json.dumps(manifest, indent=2, ensure_ascii=False)
    with open(WEB_PACKAGE_FILE, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"{serialized}\n")


def write_android_properties_version(version):
    """
    Rewrite VERSION_NAME in the android Gradle properties so the published aar tracks VERSION.txt.
    Rewriting the single line rather than the file keeps the remaining properties and their comments in place.
    """
    if not os.path.exists(ANDROID_PROPERTIES_FILE):
        raise SystemExit(f"{repo_path(ANDROID_PROPERTIES_FILE)} not found.")
    with open(ANDROID_PROPERTIES_FILE, encoding="utf-8") as handle:
        content = handle.read()
    updated, replaced = ANDROID_VERSION_REGEXP.subn(f"VERSION_NAME={version}", content)
    if replaced != 1:
        raise SystemExit(f"expected one VERSION_NAME in {repo_path(ANDROID_PROPERTIES_FILE)}, found {replaced}.")
    with open(ANDROID_PROPERTIES_FILE, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(updated)


def build_change_log():
    subprocess.run([sys.executable, CHANGE_LOG_BUILDER], cwd=REPO_ROOT, check=True)


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
    """Resolve the target version, confirm when interactive, then write, build, commit and tag."""
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
        print(f"[dry-run] files   : {', '.join(repo_path(file) for file in RELEASE_FILES)}")
        return

    if interactive and not confirm(f"release {tag}?"):
        raise SystemExit("aborted.")

    ensure_clean_worktree()
    ensure_tag_absent(tag)

    write_version(target)
    write_web_package_version(target)
    write_android_properties_version(target)
    build_change_log()

    run_git(["add", *(repo_path(file) for file in RELEASE_FILES)], capture=False)
    run_git(["commit", "-m", f"release: {tag}"], capture=False)
    run_git(["tag", "-a", tag, "-m", f"release {tag}"], capture=False)

    print(f"released {tag} ({current} -> {target}).")
    print("commit and tag created locally; push with: git push --follow-tags")


if __name__ == "__main__":
    main()
