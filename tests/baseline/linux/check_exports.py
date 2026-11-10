#!/usr/bin/env python3
"""Compare libGGPO.so's dynamic ggpo_* exports with the frozen manifest."""

import argparse
import re
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()

    symbols = subprocess.run(
        ["nm", "-D", "--defined-only", str(args.library)],
        check=True, text=True, stdout=subprocess.PIPE).stdout
    actual = sorted(set(re.findall(r"\b(ggpo_[A-Za-z0-9_]+)$", symbols,
                                   re.MULTILINE)))
    expected = sorted(line.strip() for line in
                      args.manifest.read_text(encoding="utf-8").splitlines()
                      if line.strip())
    for name in actual:
        print(f"export={name}")
    missing = sorted(set(expected) - set(actual))
    added = sorted(set(actual) - set(expected))
    for name in missing:
        print(f"missing export: {name}", file=sys.stderr)
    for name in added:
        print(f"unexpected export: {name}", file=sys.stderr)
    return 1 if missing or added else 0


if __name__ == "__main__":
    sys.exit(main())
