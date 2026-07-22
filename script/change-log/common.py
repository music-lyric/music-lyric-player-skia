import re
import subprocess
from dataclasses import dataclass
from datetime import date
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

CHANGE_LOG_FILE = "CHANGELOG.md"
CURRENT_CHANGE_LOG_FILE = "CURRENT_CHANGELOG.md"

# Types rendered in the change log, in display order.
TYPE_ORDER = ["feat", "fix", "perf", "refactor", "revert", "docs"]

TYPE_TITLE_MAP = {
    "feat": "Feature",
    "fix": "Fix",
    "perf": "Performance",
    "refactor": "Refactor",
    "revert": "Revert",
    "docs": "Document",
    "breaking": "Breaking",
}

COMMIT_MESSAGE_REGEXP = re.compile(
    r"^(feat|fix|perf|chore|docs|revert|refactor|test|release)(\([a-zA-Z0-9-_]+\))?:\s(.*)$"
)

BREAKING_CHANGE_REGEXP = re.compile(r"^breaking:\s*(.*)", re.IGNORECASE)

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


def parse_commit_message(message):
    match = COMMIT_MESSAGE_REGEXP.match(message or "")
    if not match:
        return None
    return {
        "type": match.group(1),
        "scope": match.group(2)[1:-1] if match.group(2) else None,
        "message": match.group(3),
    }


def get_repo_info():
    try:
        remote = run_git(["remote", "get-url", "origin"]).strip()
    except RuntimeError:
        return None
    match = re.search(r"(?:github\.com[:/])(.+?)/(.+?)(?:\.git)?$", remote)
    if not match:
        return None
    return {"owner": match.group(1), "name": match.group(2)}


def get_commit_info(start="", end="HEAD"):
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
    return re.sub(r"(\n\s*){2,}", "\n\n", content).strip() + "\n"


def write_output(name, content):
    with open(REPO_ROOT / name, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def build_header(version, info):
    commit_date = info.date if info else ""
    return f"## {version} ({commit_date or date.today().strftime('%Y-%m-%d')})"


def _extract_breaking_changes(body):
    changes = []
    for raw in (body or "").splitlines():
        match = BREAKING_CHANGE_REGEXP.match(raw.strip())
        if not match:
            continue
        target = match.group(1).strip()
        if target:
            changes.append(target)
    return changes


def commit_link(commit, repo):
    url = f"https://github.com/{repo['owner']}/{repo['name']}/commit/{commit.hash_full}"
    return f"[{commit.hash_short}]({url})"


def build_scope(commits, repo, is_common, breaking):
    indent = "" if is_common else "  "
    grouped = {}
    for commit in commits:
        grouped.setdefault(commit.message, []).append(commit)
        breaking.extend(_extract_breaking_changes(commit.body))

    lines = []
    for message, items in grouped.items():
        links = ", ".join(commit_link(item, repo) for item in items)
        lines.append(f"{indent}- {message} ({links})")
    return lines


def build_type_contents(scopes, repo):
    lines = []
    breaking = []

    common = scopes.pop("common", None)
    if common:
        lines.extend(build_scope(common, repo, True, breaking))

    for scope in sorted(scopes):
        lines.append(f"- `{scope}`")
        lines.extend(build_scope(scopes[scope], repo, False, breaking))

    if breaking:
        lines += ["\n", f"### {TYPE_TITLE_MAP['breaking']}", "\n"]
        lines.extend(f"- {change}" for change in breaking)

    return lines


def build_contents(infos, repo):
    type_map = {}
    for info in infos:
        if info.type not in TYPE_ORDER:
            continue
        scope_map = type_map.setdefault(info.type, {})
        scope_map.setdefault(info.scope or "common", []).append(info)

    lines = []
    for type_name in TYPE_ORDER:
        scopes = type_map.get(type_name)
        if not scopes:
            continue
        lines += ["\n", f"### {TYPE_TITLE_MAP[type_name]}", "\n"]
        lines.extend(build_type_contents(scopes, repo))
    return lines
