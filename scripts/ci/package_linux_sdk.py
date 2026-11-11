#!/usr/bin/env python3
"""Create and verify raw, DEB, and RPM Linux SDK packages."""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


# Run a command visibly and fail on its first nonzero result.
def run(command, **kwargs):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


# Read one typed value from the generated CMake cache.
def cache_value(cache, name):
    match = re.search(
        rf"^{re.escape(name)}:[^=]+=(.*)$",
        cache.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if not match:
        raise RuntimeError(f"{name} is missing from {cache}")
    return match.group(1).strip()


# Extract one package and return the SDK prefix visible to consumers.
def extract_package(kind, package, destination, base_name):
    destination.mkdir(parents=True, exist_ok=True)
    if kind == "raw":
        run(["tar", "-xzf", package, "-C", destination])
        return destination / base_name
    if kind == "deb":
        run(["dpkg-deb", "--extract", package, destination])
        return destination / "usr"

    cpio_archive = destination.parent / "package.cpio"
    with cpio_archive.open("wb") as output:
        run(["rpm2cpio", package], stdout=output)
    with cpio_archive.open("rb") as source:
        run(["cpio", "-idm", "--quiet"], cwd=destination, stdin=source)
    cpio_archive.unlink()
    return destination / "usr"


# Compile and run installed consumers, then audit the extracted SDK ABI.
def verify_prefix(kind, prefix, source, checks, cc, cxx, jobs):
    required = [
        prefix / "include/ggponet.h",
        prefix / "lib/cmake/GGPO/GGPOConfig.cmake",
    ]
    if not all(path.is_file() for path in required):
        raise RuntimeError(f"{kind} package is missing SDK files under {prefix}")

    build = checks / kind / "consumer"
    run([
        "python3", source / "tests/consumer/verify_consumers.py",
        "--prefix", prefix, "--work", build, "--generator", "Ninja",
        "--cc", cc, "--cxx", cxx, "--jobs", str(jobs),
    ])
    run(["python3", source / "scripts/ci/audit_linux_sdk.py", prefix])


# Package one configured linkage and validate every distributed format.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--install", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--jobs", type=int, default=1)
    args = parser.parse_args()

    cache = args.build.resolve() / "CMakeCache.txt"
    version = cache_value(cache, "CMAKE_PROJECT_VERSION")
    shared = cache_value(cache, "GGPO_BUILD_SHARED") == "ON"
    static = cache_value(cache, "GGPO_BUILD_STATIC") == "ON"
    # Must match CPACK_PACKAGE_FILE_NAME in src/cmake/linux_packaging.cmake.
    suffix = "" if shared and static else ("-shared" if shared else "-static")
    base_name = f"ggpo-{version}-linux-x86_64{suffix}"
    packages = args.out.resolve()
    checks = packages.parent / "package-checks"
    shutil.rmtree(packages, ignore_errors=True)
    shutil.rmtree(checks, ignore_errors=True)
    packages.mkdir(parents=True)

    raw_root = packages / base_name
    shutil.copytree(args.install.resolve(), raw_root, symlinks=True)
    raw_package = packages / f"{base_name}.tar.gz"
    run(["tar", "-C", packages, "-czf", raw_package, base_name])
    shutil.rmtree(raw_root)

    config = args.build.resolve() / "CPackConfig.cmake"
    run(["cpack", "-G", "DEB", "-B", packages, "--config", config])
    run(["cpack", "-G", "RPM", "-B", packages, "--config", config])

    artifacts = {
        "raw": raw_package,
        "deb": packages / f"{base_name}.deb",
        "rpm": packages / f"{base_name}.rpm",
    }
    for kind, package in artifacts.items():
        if not package.is_file():
            raise RuntimeError(f"missing {kind} package: {package}")
        prefix = extract_package(kind, package, checks / kind / "root", base_name)
        verify_prefix(
            kind, prefix, args.source.resolve(), checks,
            args.cc, args.cxx, args.jobs,
        )
        print(f"Verified {kind}: {package}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Linux SDK packaging failed: {error}", file=sys.stderr)
        raise SystemExit(1)
