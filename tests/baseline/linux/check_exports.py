#!/usr/bin/env python3
"""Compare the shared library's dynamic ggpo_* exports with the frozen manifest.

ELF (libGGPO.so) and Mach-O (libGGPO.dylib, whose C symbols carry a leading
underscore) share one manifest.
"""

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

    if sys.platform == "darwin":
        command, prefix = ["nm", "-gU", str(args.library)], r"\b_"
    else:
        command, prefix = ["nm", "-D", "--defined-only", str(args.library)], r"\b"
    symbols = subprocess.run(command, check=True, text=True,
                             stdout=subprocess.PIPE).stdout
    actual = sorted(set(re.findall(prefix + r"(ggpo_[A-Za-z0-9_]+)$", symbols,
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
