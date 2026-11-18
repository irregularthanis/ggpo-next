#!/usr/bin/env python3
"""Audit macOS binaries: every architecture, and the minimum macOS each records.

Each Mach-O file (executable, dylib, or static archive) under the given paths
must contain every --arch slice, and every object in every slice must record
--minos as its minimum macOS version (LC_BUILD_VERSION minos, or the older
LC_VERSION_MIN_MACOSX). A higher minos would refuse to load on the oldest
supported macOS, which no CI runner can execute, so this check stands in for
running there.
"""

import argparse
import pathlib
import re
import subprocess
import sys

MACHO_MAGICS = {
    b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",   # thin, little-endian
    b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf",   # universal (fat)
}


def is_macho(path):
    with path.open("rb") as f:
        head = f.read(8)
    return head[:4] in MACHO_MAGICS or head == b"!<arch>\n"


def run(*command):
    return subprocess.run(command, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout


def minimum_versions(path, arch):
    """Every minimum-OS version recorded in one slice (one per object)."""
    commands = run("otool", "-arch", arch, "-l", str(path))
    versions = re.findall(r"^\s*minos (\S+)$", commands, re.MULTILINE)
    # LC_VERSION_MIN_MACOSX: "cmd ..." then "version X" on a later line.
    for block in commands.split("Load command")[1:]:
        if "LC_VERSION_MIN_MACOSX" in block:
            versions += re.findall(r"^\s*version (\S+)$", block, re.MULTILINE)
    return versions


def audit(path, archs, minos):
    errors = []
    present = run("lipo", "-archs", str(path)).split()
    for arch in archs:
        if arch not in present:
            errors.append(f"{path}: missing {arch} slice (has {' '.join(present)})")
            continue
        versions = minimum_versions(path, arch)
        if not versions:
            errors.append(f"{path} [{arch}]: records no minimum macOS version")
        wrong = sorted(set(v for v in versions if v != minos))
        if wrong:
            errors.append(f"{path} [{arch}]: minimum macOS {', '.join(wrong)}, expected {minos}")
        else:
            print(f"ok {path} [{arch}] minos {minos} ({len(versions)} object(s))")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="+", type=pathlib.Path)
    parser.add_argument("--arch", action="append", required=True, dest="archs")
    parser.add_argument("--minos", required=True)
    args = parser.parse_args()

    files = []
    for root in args.paths:
        candidates = [root] if root.is_file() else sorted(root.rglob("*"))
        files += [p for p in candidates
                  if p.is_file() and not p.is_symlink() and is_macho(p)]
    if not files:
        print("no Mach-O files found", file=sys.stderr)
        return 1
    errors = []
    for path in files:
        errors += audit(path, args.archs, args.minos)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
