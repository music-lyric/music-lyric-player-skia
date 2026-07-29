"""
Shared change log helpers: reads conventional commits out of git and renders them as markdown sections.
"""

import re
import subprocess

from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

CHANGE_LOG_FILE = "CHANGELOG.md"
CURRENT_CHANGE_LOG_FILE = "CURRENT_CHANGELOG.md"

# Types rendered in the change log, in display order.
TYPE_ORDER = ["feat", "fix", "perf", "refactor", "revert", "docs"]

# Scopes dropped from the change log; their commits do not ship in any released artifact.
EXCLUDED_SCOPES = {"example"}

# Body trailers that open a section of their own, in display order.
MARKER_ORDER = ["breaking", "security", "deprecated"]

TYPE_TITLE_MAP = {
    "feat": "Feature",
    "fix": "Fix",
    "perf": "Performance",
    "refactor": "Refactor",
    "revert": "Revert",
    "docs": "Document",
    "breaking": "Breaking",
    "security": "Security",
    "deprecated": "Deprecated",
}

COMMIT_MESSAGE_REGEXP = re.compile(
    r"^(feat|fix|perf|chore|docs|revert|refactor|test|release)(\([a-zA-Z0-9-_]+\))?:\s(.*)$"
)

# One note per line; repeat the key to record several notes on the same commit.
MARKER_TRAILER_REGEXP = re.compile(rf"^({'|'.join(MARKER_ORDER)}):\s*(.+)$", re.IGNORECASE)

# Issue trailers render inline after the commit links; `closes` is also what GitHub acts on.
ISSUE_TRAILER_REGEXP = re.compile(r"^(refs|closes):\s*(.+)$", re.IGNORECASE)

# Issue trailers hold bare numbers, so one line may carry several of them.
ISSUE_NUMBER_REGEXP = re.compile(r"#(\d+)")

# Fields are joined by an ASCII unit separator; NUL (via -z) separates whole records.
FIELD_SEP = "\x1f"
LOG_FORMAT = FIELD_SEP.join(["%h", "%H", "%an", "%ad", "%s", "%b"])


@dataclass
class CommitInfo:
    type: str
    scope: "str | None"
    message: str
    hash_short: str
    hash_full: str
    author: str
    date: str
    subject: str
    body: str
    markers: dict = field(default_factory=dict)
    refs: list = field(default_factory=list)
    closes: list = field(default_factory=list)


def run_git(args):
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        raise RuntimeError(f"failed: git {' '.join(args)}\n{result.stderr.strip()}")
    return result.stdout


def project_version():
    version_file = REPO_ROOT / "VERSION.txt"
    if not version_file.exists():
        return "0.0.0"
    version = version_file.read_text(encoding="utf-8").strip()
    return version or "0.0.0"


def unique(values):
    """Drop duplicates while keeping the order in which the values were first seen."""
    return list(dict.fromkeys(values))


def parse_commit_message(message):
    """Split a conventional commit subject into type, scope and message; None when it does not match."""
    match = COMMIT_MESSAGE_REGEXP.match(message or "")
    if not match:
        return None
    return {
        "type": match.group(1),
        "scope": match.group(2)[1:-1] if match.group(2) else None,
        "message": match.group(3),
    }


def parse_commit_trailers(body):
    """Collect the `key: value` trailers of a commit body into its marker notes and issue references."""
    markers = {}
    issues = {"refs": [], "closes": []}
    for raw in (body or "").splitlines():
        line = raw.strip()
        marker = MARKER_TRAILER_REGEXP.match(line)
        if marker:
            markers.setdefault(marker.group(1).lower(), []).append(marker.group(2).strip())
            continue
        issue = ISSUE_TRAILER_REGEXP.match(line)
        if issue:
            issues[issue.group(1).lower()].extend(ISSUE_NUMBER_REGEXP.findall(issue.group(2)))
    return markers, unique(issues["refs"]), unique(issues["closes"])


def get_repo_info():
    """Read owner and name out of the origin remote, or None when it is missing or not a GitHub URL."""
    try:
        remote = run_git(["remote", "get-url", "origin"]).strip()
    except RuntimeError:
        return None
    match = re.search(r"(?:github\.com[:/])(.+?)/(.+?)(?:\.git)?$", remote)
    if not match:
        return None
    return {"owner": match.group(1), "name": match.group(2)}


def get_commit_info(start="", end="HEAD"):
    """Read the conventional commits in a revision range, dropping anything that does not parse."""
    if not start and end:
        range_args = [end]
    elif start or end:
        range_args = [f"{start}..{end}"]
    else:
        range_args = []
    try:
        out = run_git(["log", *range_args, "-z", f"--pretty=format:{LOG_FORMAT}", "--date=short"])
    except RuntimeError:
        return []

    commits = []
    for record in filter(None, out.split("\0")):
        fields = record.split(FIELD_SEP, 5)
        if len(fields) < 6:
            continue
        short_hash, full_hash, author, commit_date, subject, body = fields
        parsed = parse_commit_message(subject)
        if not parsed:
            continue
        markers, refs, closes = parse_commit_trailers(body)
        commits.append(
            CommitInfo(
                type=parsed["type"],
                scope=parsed["scope"],
                message=parsed["message"],
                hash_short=short_hash,
                hash_full=full_hash,
                author=author,
                date=commit_date,
                subject=subject,
                body=body.strip(),
                markers=markers,
                refs=refs,
                closes=closes,
            )
        )
    return commits


def get_latest_tag():
    try:
        tag = run_git(["describe", "--tags", "--abbrev=0"]).strip()
    except RuntimeError:
        return None
    return tag or None


def get_all_tags():
    try:
        out = run_git(["tag", "--sort=-version:refname"])
    except RuntimeError:
        return []
    return [line for line in out.splitlines() if line]


def format_result(content):
    """Collapse runs of blank lines and end the document with exactly one newline."""
    return re.sub(r"(\n\s*){2,}", "\n\n", content).strip() + "\n"


def write_output(name, content):
    with open(REPO_ROOT / name, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def build_header(version, info):
    """Render the version heading, dated from the newest commit or today when there is none."""
    commit_date = info.date if info else ""
    return f"## {version} ({commit_date or date.today().strftime('%Y-%m-%d')})"


def commit_link(commit, repo):
    url = f"https://github.com/{repo['owner']}/{repo['name']}/commit/{commit.hash_full}"
    return f"[{commit.hash_short}]({url})"


def issue_link(number, repo):
    url = f"https://github.com/{repo['owner']}/{repo['name']}/issues/{number}"
    return f"[#{number}]({url})"


def build_issue_suffix(commits, repo):
    """Render the `refs` and `closes` trailers of one bullet's commits as a trailing clause each."""
    suffix = ""
    for name in ("refs", "closes"):
        numbers = unique(number for commit in commits for number in getattr(commit, name))
        if numbers:
            suffix += f", {name} " + " ".join(issue_link(number, repo) for number in numbers)
    return suffix


def build_scope(commits, repo, is_common):
    """Render one scope's commits as bullets, merging the ones that share a message."""
    indent = "" if is_common else "  "
    grouped = {}
    for commit in commits:
        grouped.setdefault(commit.message, []).append(commit)

    lines = []
    for message, items in grouped.items():
        links = ", ".join(commit_link(item, repo) for item in items)
        lines.append(f"{indent}- {message} ({links}){build_issue_suffix(items, repo)}")
    return lines


def build_type_contents(scopes, repo):
    """Render every scope of one commit type, with the unscoped ones first."""
    lines = []

    common = scopes.pop("common", None)
    if common:
        lines.extend(build_scope(common, repo, True))

    for scope in sorted(scopes):
        lines.append(f"- `{scope}`")
        lines.extend(build_scope(scopes[scope], repo, False))

    return lines


def build_marker_contents(infos, repo):
    """Render one section per marker trailer, each note carrying a link back to its commit."""
    lines = []
    for marker in MARKER_ORDER:
        notes = [(note, info) for info in infos for note in info.markers.get(marker, [])]
        if not notes:
            continue
        lines += ["\n", f"### {TYPE_TITLE_MAP[marker]}", "\n"]
        lines.extend(f"- {note} ({commit_link(info, repo)})" for note, info in notes)
    return lines


def build_contents(infos, repo):
    """Render the marker sections first, then group the commits by type and scope in display order."""
    included = [info for info in infos if info.scope not in EXCLUDED_SCOPES]

    type_map = {}
    for info in included:
        if info.type not in TYPE_ORDER:
            continue
        scope_map = type_map.setdefault(info.type, {})
        scope_map.setdefault(info.scope or "common", []).append(info)

    lines = build_marker_contents(included, repo)
    for type_name in TYPE_ORDER:
        scopes = type_map.get(type_name)
        if not scopes:
            continue
        lines += ["\n", f"### {TYPE_TITLE_MAP[type_name]}", "\n"]
        lines.extend(build_type_contents(scopes, repo))
    return lines
