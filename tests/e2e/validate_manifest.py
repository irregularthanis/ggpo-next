#!/usr/bin/env python3
"""Require one passing report for every native platform/role assignment."""

import argparse
import json
from pathlib import Path

from run_matrix import generate_matrix, validate_matrix


def build_manifest(root):
    """Validate one passing report per scenario and return the manifest."""
    root = Path(root)
    reports = []
    for path in root.rglob("report.json"):
        report = json.loads(path.read_text(encoding="utf-8"))
        if report.get("result") != "pass":
            raise ValueError(f"non-passing report: {path}")
        reports.append(report)
    scenarios = [report.get("scenario") for report in reports]
    expected = [item["scenario"] for item in validate_matrix(generate_matrix())]
    if len(scenarios) != len(set(scenarios)):
        raise ValueError(f"duplicate scenario reports: {scenarios}")
    if set(scenarios) != set(expected):
        raise ValueError(
            f"incomplete scenario reports: expected={expected}, actual={scenarios}"
        )
    return {"schema": 1, "scenarios": sorted(scenarios),
            "reports": reports, "result": "pass"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    manifest = build_manifest(args.root)
    rendered = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    print(rendered, end="")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(rendered, encoding="utf-8")


if __name__ == "__main__":
    main()
