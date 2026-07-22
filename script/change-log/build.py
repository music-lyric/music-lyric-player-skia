#!/usr/bin/env python
"""
Generate the change log from conventional commits.

Default mode writes the full CHANGELOG.md; --current writes CURRENT_CHANGELOG.md for the latest tagged range only.

Usage: build.py [--current] [--show-current-head]
"""

import argparse

from common import (
    CHANGE_LOG_FILE,
    CURRENT_CHANGE_LOG_FILE,
    build_contents,
    build_header,
    format_result,
    get_all_tags,
    get_commit_info,
    get_repo_info,
    project_version,
    write_output,
)


def handle_build(start, end, repo, show_head=True):
    commits = get_commit_info(start, end)

    # Commits above the first release commit belong to the next, still-unreleased version.
    key = f"v{project_version()}"
    wait = {}

    for item in commits:
        if item.type == "release":
            existing = wait.get(item.message)
            wait[item.message] = {"head": item, "list": existing["list"] if existing else []}
            key = item.message
            continue
        wait.setdefault(key, {"head": None, "list": []})["list"].append(item)

    lines = []
    for version, content in wait.items():
        lines.append("\n")
        if show_head:
            lines.append(build_header(version, content["head"]))
        lines.extend(build_contents(content["list"], repo))

    return format_result("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description="Generate the change log from conventional commits.")
    parser.add_argument("--current", action="store_true", help="write only the latest tagged version")
    parser.add_argument(
        "--show-current-head", action="store_true", help="include the version heading in --current output"
    )
    args = parser.parse_args()

    repo = get_repo_info()
    if not repo:
        raise SystemExit("get repo info failed.")

    if args.current:
        tags = get_all_tags()
        if not tags:
            return
        old = tags[1] if len(tags) > 1 else ""
        result = handle_build(old, tags[0], repo, args.show_current_head)
        target = CURRENT_CHANGE_LOG_FILE
    else:
        result = f"# Changelog\n\n{handle_build('', '', repo)}"
        target = CHANGE_LOG_FILE

    write_output(target, result)


if __name__ == "__main__":
    main()
