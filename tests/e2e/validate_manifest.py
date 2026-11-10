#!/usr/bin/env python3
"""Require one passing report for every native platform/role assignment."""

import argparse
import json
from pathlib import Path

from run_matrix import generate_matrix, validate_matrix


def build_manifest(root):
    """Validate one passing report per fixture/scenario and return the manifest."""
    root = Path(root)
    reports = []
    for path in root.rglob("report.json"):
        report = json.loads(path.read_text(encoding="utf-8"))
        if report.get("result") != "pass":
            raise ValueError(f"non-passing report: {path}")
        reports.append(report)
    base = [item["scenario"] for item in validate_matrix(generate_matrix())]
    expected = ({("smoke", scenario) for scenario in base} |
                {("vectorwar", f"vectorwar-{scenario}") for scenario in base})
    identities = [
        (report.get("fixture"), report.get("scenario")) for report in reports
    ]
    if len(identities) != len(set(identities)):
        raise ValueError(f"duplicate fixture/scenario reports: {identities}")
    if set(identities) != expected:
        raise ValueError(
            f"incomplete fixture/scenario reports: "
            f"expected={sorted(expected)}, actual={identities}"
        )
    return {"schema": 1, "scenarios": sorted(identities),
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
