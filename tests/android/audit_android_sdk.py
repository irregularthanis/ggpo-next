#!/usr/bin/env python3
"""Audit an installed Android GGPO SDK for one ABI.

Android counterpart of scripts/ci/audit_linux_sdk.py:

- libGGPO.so has SONAME libGGPO.so and no versioned files or symlinks;
- every LOAD segment is aligned to at least 16 KB (Google Play requirement);
- NEEDED lists only Android system libraries (no libc++_shared.so);
- the dynamic ggpo_* exports equal the frozen manifest;
- API-level ceiling: every undefined symbol resolves in the NDK system-library
  stubs for the minimum API level, so the library loads on that release.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

SYSTEM_LIBRARIES = {"libc.so", "libm.so", "libdl.so", "liblog.so"}
TRIPLES = {
    "arm64-v8a": "aarch64-linux-android",
    "armeabi-v7a": "arm-linux-androideabi",
    "x86_64": "x86_64-linux-android",
    "x86": "i686-linux-android",
}


def output(*command):
    return subprocess.run([str(part) for part in command], check=True, text=True,
                          stdout=subprocess.PIPE).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--ndk", type=Path, required=True)
    parser.add_argument("--abi", required=True, choices=sorted(TRIPLES))
    parser.add_argument("--api", type=int, default=21)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    tools = args.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    readelf = tools / "llvm-readelf"
    nm = tools / "llvm-nm"
    lib_dir = args.prefix / "lib"
    library = lib_dir / "libGGPO.so"
    errors = []

    if not library.is_file() or library.is_symlink():
        raise SystemExit(f"missing or symlinked {library}")
    extra = sorted(p.name for p in lib_dir.glob("libGGPO.so.*"))
    if extra:
        errors.append(f"versioned library files present: {extra}")
    if not (lib_dir / "libGGPO.a").is_file():
        errors.append("missing libGGPO.a")

    dynamic = output(readelf, "-d", library)
    soname = re.search(r"\(SONAME\).*\[(.+?)\]", dynamic)
    if not soname or soname.group(1) != "libGGPO.so":
        errors.append(f"unexpected SONAME: {soname.group(1) if soname else None}")
    needed = sorted(re.findall(r"\(NEEDED\).*\[(.+?)\]", dynamic))
    unexpected = [name for name in needed if name not in SYSTEM_LIBRARIES]
    if unexpected:
        errors.append(f"non-system dependencies: {unexpected}")

    alignments = [int(value, 16) for value in re.findall(
        r"^\s*LOAD\s+(?:\S+\s+){5}\S+\s+(0x[0-9a-fA-F]+)\s*$",
        output(readelf, "-lW", library), re.MULTILINE)]
    if not alignments or min(alignments) < 0x4000:
        errors.append(f"LOAD alignment below 16 KB: {[hex(a) for a in alignments]}")

    defined = output(nm, "-D", "--defined-only", library)
    exports = sorted(set(re.findall(r"\b(ggpo_[A-Za-z0-9_]+)$", defined, re.MULTILINE)))
    expected = sorted(line.strip() for line in
                      args.manifest.read_text(encoding="utf-8").splitlines()
                      if line.strip())
    if exports != expected:
        errors.append(f"exports differ: missing={sorted(set(expected) - set(exports))}"
                      f" added={sorted(set(exports) - set(expected))}")

    stubs_dir = (args.ndk / "toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib"
                 / TRIPLES[args.abi] / str(args.api))
    available = set()
    for name in needed:
        stub = stubs_dir / name
        if not stub.is_file():
            errors.append(f"no API {args.api} stub for {name} at {stub}")
            continue
        # Stub symbols carry version suffixes (abort@@LIBC); compare base names.
        for symbol in re.findall(r"^\S*\s*[TWDBRVi]\s+(\S+)$",
                                 output(nm, "-D", "--defined-only", stub),
                                 re.MULTILINE):
            available.add(symbol.split("@", 1)[0])
    undefined = set(re.findall(r"^\s+U\s+(\S+?)(?:@.*)?$",
                               output(nm, "-D", "--undefined-only", library),
                               re.MULTILINE))
    missing = sorted(symbol for symbol in undefined
                     if symbol not in available and not symbol.startswith("__cxa_")
                     and symbol not in ("__gxx_personality_v0",))
    if missing:
        errors.append(f"symbols not available at API {args.api}: {missing}")

    report = {"abi": args.abi, "api": args.api, "soname": soname.group(1) if soname else None,
              "needed": needed, "min_load_alignment": hex(min(alignments)) if alignments else None,
              "ggpo_exports": exports, "undefined_symbols": len(undefined),
              "errors": errors}
    rendered = json.dumps(report, indent=2) + "\n"
    print(rendered, end="")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered, encoding="utf-8")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
