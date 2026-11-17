#!/usr/bin/env python3
"""Map changed paths to the CI areas they affect.

Each compatibility workflow asks whether *its* area is affected and skips its
expensive jobs otherwise; its aggregate check still reports, so required
checks never wait. Rules are ordered, most specific first, and the first
matching rule decides a path. Anything unmatched affects every area, so a new
directory can never silently skip testing. Pushes to master and manual runs
bypass this and run everything (see classify-changes.yml).
"""

import argparse
import fnmatch
import subprocess
import sys

PLATFORMS = ("linux", "windows", "mingw", "android")
AREAS = PLATFORMS + ("e2e", "nat", "infra")
ALL = AREAS
CORE = PLATFORMS + ("e2e", "nat")

# Which area each workflow file belongs to; editing a workflow tests that area.
WORKFLOW_AREAS = {
    "android-compatibility.yml": ("android",),
    "cross-platform-e2e.yml": ("e2e",),
    "linux-compatibility.yml": ("linux",),
    "mingw-compatibility.yml": ("mingw",),
    "nat-traversal.yml": ("nat",),
    "windows-compatibility.yml": ("windows",),
    "infrastructure.yml": ("infra",),
    "deploy-matchmaking.yml": ("infra",),
    "public-multiplatform-game.yml": ("infra",),
    "sanitizer-qualification.yml": ("linux", "windows", "android"),
    "owner-approval.yml": (),
}

WIN32_FRONTEND = ("win32_*", "gdi_*", "main.cpp", "ggpo_perfmon.*", "*.rc",
                  "Resource.h", "*.ico", "targetver.h")
SDL_FRONTEND = ("sdl_*", "vw_sdl.h")

RULES = (
    # Documentation never needs a build.
    (("doc/*", "docs/*", "*.md"), ()),
    # The classifier itself and these rules: test everything.
    ((".github/ci/*", ".github/workflows/classify-changes.yml"), ALL),
    # One adapter per OS family.
    (("src/lib/ggpo/platform_posix.cpp",), ("linux", "android", "e2e", "nat")),
    (("src/lib/ggpo/platform_windows.cpp", "src/lib/ggpo/platform_windows.h"),
     ("windows", "mingw", "e2e")),
    # Shared core, public header and build system.
    (("src/lib/*", "src/include/*", "src/cmake/*", "CMakeLists.txt",
      "src/CMakeLists.txt", "src/CMakeSources.cmake", "cmake/*"), CORE),
    (("src/platform/linux/*",), ("linux", "e2e", "nat")),
    (("src/platform/windows/*",), ("windows", "mingw", "e2e")),
    (("src/platform/android/*",), ("android", "e2e")),
    # Vector War frontends, then its portable core.
    (("src/apps/vectorwar/android/*",), ("android",)),
    (tuple("src/apps/vectorwar/" + p for p in WIN32_FRONTEND), ("windows",)),
    (tuple("src/apps/vectorwar/" + p for p in SDL_FRONTEND), ("linux", "android")),
    (("src/apps/vectorwar/*",), CORE),
    # Tests.
    (("tests/android/*",), ("android",)),
    (("tests/nat/*",), ("nat",)),
    (("tests/e2e/*",), CORE),
    (("tests/*",), CORE),
    # Lobby/relay server: its own workflow, NAT emulation, and the CTest
    # registration that runs on Linux.
    (("services/matchmaking/*",), ("infra", "nat", "linux")),
    (("infra/*",), ("infra",)),
    (("docker/*", "scripts/linux/*"), ("linux", "nat")),
    (("scripts/ci/*windows*", "scripts/ci/*.ps1", "bin/*.cmd",
      "configure_windows.cmd", "appveyor.yml"), ("windows",)),
    (("scripts/ci/*linux*",), ("linux",)),
)


def areas_for(path):
    """Return the set of areas one changed path affects."""
    if path.startswith(".github/workflows/"):
        name = path.rsplit("/", 1)[-1]
        if name in WORKFLOW_AREAS:
            return set(WORKFLOW_AREAS[name])
    for patterns, areas in RULES:
        if any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns):
            return set(areas)
    return set(ALL)


def affected(paths):
    result = set()
    for path in paths:
        result |= areas_for(path)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--area", required=True, choices=AREAS)
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", required=True)
    args = parser.parse_args(argv)
    paths = subprocess.run(["git", "diff", "--name-only", args.base, args.head],
                           check=True, capture_output=True, text=True).stdout.split()
    found = affected(paths)
    print(f"{len(paths)} changed paths affect: {', '.join(sorted(found)) or 'nothing'}",
          file=sys.stderr)
    print("true" if args.area in found else "false")


if __name__ == "__main__":
    main()
