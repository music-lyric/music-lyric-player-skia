#!/usr/bin/env python
"""
Extract a single version's section out of CHANGELOG.md into CURRENT_CHANGELOG.md.

Usage: match.py [--version <version>] [--include-header]
"""

import argparse
import re

from common import (
    CHANGE_LOG_FILE,
    CURRENT_CHANGE_LOG_FILE,
    REPO_ROOT,
    get_latest_tag,
    project_version,
    write_output,
)


def match_change_log_by_version(content, version, include_header=False):
    target = re.sub(r"^v", "", version, flags=re.IGNORECASE).replace(".", r"\.")
    regex = re.compile(rf"(##\s*v{target}.*)[\r\n]+([\s\S]*?)(?=##\s*v|$)")
    match = regex.search(content)
    if not match:
        return ""
    header, body = match.group(1), match.group(2)
    if include_header:
        return f"{header.strip()}\n\n{body.strip()}"
    return body.strip()


def main():
    parser = argparse.ArgumentParser(description="Extract one version from the change log.")
    parser.add_argument("--version", default="", help="version to extract; defaults to the latest tag")
    parser.add_argument("--include-header", action="store_true", help="keep the version heading in the output")
    args = parser.parse_args()

    version = args.version or get_latest_tag() or project_version()
    if not version:
        return

    source = REPO_ROOT / CHANGE_LOG_FILE
    if not source.exists():
        return
    content = source.read_text(encoding="utf-8")
    if not content:
        return

    result = match_change_log_by_version(content, version, args.include_header)
    write_output(CURRENT_CHANGE_LOG_FILE, result)


if __name__ == "__main__":
    main()
