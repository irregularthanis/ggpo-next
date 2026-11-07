#!/usr/bin/env python3
"""Audit an installed Linux GGPO SDK without changing its contents."""

import argparse
import json
import re
import subprocess
from pathlib import Path


def output(*command):
    return subprocess.run(command, check=True, text=True,
                          stdout=subprocess.PIPE).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--max-glibc", default="2.39")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    library_dir = args.prefix.resolve() / "lib"
    shared = sorted(library_dir.glob("libGGPO.so.*.*.*"))
    static = library_dir / "libGGPO.a"
    if not shared and not static.is_file():
        parser.error(f"no GGPO library found under {library_dir}")

    report = {"prefix": str(args.prefix), "max_glibc_allowed": args.max_glibc}
    if shared:
        library = shared[-1]
        version_text = output("readelf", "--version-info", str(library))
        versions = sorted({tuple(map(int, match)) for match in
                           re.findall(r"GLIBC_(\d+)\.(\d+)", version_text)})
        maximum = max(versions, default=(0, 0))
        allowed = tuple(map(int, args.max_glibc.split(".")))
        if maximum > allowed:
            raise SystemExit(
                f"{library.name} requires GLIBC_{maximum[0]}.{maximum[1]}, "
                f"newer than {args.max_glibc}"
            )
        dynamic = output("readelf", "-d", str(library))
        if "Library soname: [libGGPO.so.0]" not in dynamic:
            raise SystemExit(f"unexpected or missing SONAME in {library.name}")
        symbols = output("nm", "-D", "--defined-only", str(library))
        exports = sorted(set(re.findall(r"\b(ggpo_[A-Za-z0-9_]+)$", symbols,
                                        re.MULTILINE)))
        if not exports:
            raise SystemExit(f"no public ggpo_* symbols exported by {library.name}")
        report.update({
            "library": library.name,
            "max_glibc_required": f"{maximum[0]}.{maximum[1]}",
            "soname": "libGGPO.so.0",
            "ggpo_exports": exports,
        })
    else:
        report.update({"library": static.name, "archive": True})

    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    print(rendered, end="")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered, encoding="utf-8")


if __name__ == "__main__":
    main()
