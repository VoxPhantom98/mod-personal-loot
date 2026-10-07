#!/usr/bin/env python3
"""Check and apply the pinned personal-loot patches. Never touches databases."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

PACKAGE = Path(__file__).resolve().parents[1]


def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)


def check(root, spec):
    patch = PACKAGE / spec["patch"]
    if hashlib.sha256(patch.read_bytes()).hexdigest() != spec["sha256"]:
        raise RuntimeError(f"Patch checksum mismatch: {patch.name}")
    head = git(root, "rev-parse", "HEAD")
    if head.returncode or head.stdout.strip() != spec["commit"]:
        raise RuntimeError(f"{root}: expected HEAD {spec['commit']}; use a clean supported checkout")
    stats = git(root, "apply", "--numstat", str(patch))
    if stats.returncode:
        raise RuntimeError(stats.stderr.strip())
    paths = [line.split("\t", 2)[2] for line in stats.stdout.splitlines()]
    reverse = git(root, "apply", "--reverse", "--check", str(patch))
    if reverse.returncode == 0:
        print(f"Already applied: {patch.name}")
        return None
    changes = git(root, "status", "--porcelain", "--untracked-files=all", "--", *paths)
    if changes.returncode or changes.stdout.strip():
        raise RuntimeError(f"{root}: patch targets have local changes; preserve them and use a clean checkout")
    result = git(root, "apply", "--check", str(patch))
    if result.returncode:
        raise RuntimeError(result.stderr.strip())
    print(f"Ready: {patch.name}")
    return root, patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", required=True, type=Path, help="AzerothCore checkout containing modules/")
    parser.add_argument("--dry-run", action="store_true", help="Validate only; do not apply patches")
    args = parser.parse_args()
    core = args.core.resolve()
    spec = json.loads((PACKAGE / "compatibility.json").read_text(encoding="utf-8"))
    jobs = [check(core, spec["core"])]
    bots = core / "modules" / "mod-playerbots"
    if bots.exists():
        jobs.append(check(bots, spec["playerbots"]))
    else:
        print("Playerbots not installed; optional bot patch skipped.")
    if args.dry_run:
        print("Checks passed. No files changed.")
        return
    for job in jobs:
        if job:
            root, patch = job
            result = git(root, "apply", str(patch))
            if result.returncode:
                raise RuntimeError(result.stderr.strip())
            print(f"Applied: {patch.name}")
    print("Patches ready. Install SQL, configure, and rebuild using the README.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
