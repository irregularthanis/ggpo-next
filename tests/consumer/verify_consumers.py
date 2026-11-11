#!/usr/bin/env python3
"""Build and run the installed-SDK consumers for every installed linkage.

For each case available in the prefix (default, static, explicit shared) this
configures tests/consumer, builds the C11 and C++98 programs, runs them, and
proves the binary's real dependency on the GGPO shared library:

- Linux: readelf -d must (shared) or must not (static) list libGGPO.so.
- Windows: the static programs must run with the SDK's bin directory removed
  from PATH; shared programs run with it added.
- Android (GGPO_TEST_LAUNCHER set): programs run on the device through the adb
  launcher; libGGPO.so is pushed only for shared cases, so a static program
  that still needed it would fail to load. NEEDED is checked as on Linux.
"""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

WINDOWS = platform.system() == "Windows"
PROGRAMS = ("ggpo_installed_c11", "ggpo_installed_cpp98")


def run(command, **kwargs):
    print("+", " ".join(str(part) for part in command), flush=True)
    return subprocess.run([str(part) for part in command], check=True, **kwargs)


def installed_flavors(prefix):
    config = prefix / "lib/cmake/GGPO"
    flavors = []
    if (config / "GGPOSharedTargets.cmake").is_file():
        flavors.append("shared")
    if (config / "GGPOStaticTargets.cmake").is_file():
        flavors.append("static")
    if not flavors:
        raise SystemExit(f"no GGPO exports found under {config}")
    return flavors


def cases_for(flavors):
    default = "shared" if "shared" in flavors else "static"
    cases = [("default", default)]
    if "static" in flavors:
        cases.append(("static", "static"))
    if "shared" in flavors:
        cases.append(("shared", "shared"))
    return cases


def executable(build, config, name):
    suffix = ".exe" if WINDOWS else ""
    for candidate in (build / config / f"{name}{suffix}", build / f"{name}{suffix}"):
        if candidate.is_file():
            return candidate
    raise SystemExit(f"{name} was not built under {build}")


def needs_shared_ggpo(program):
    dynamic = subprocess.run(["readelf", "-d", str(program)], check=True,
                             text=True, stdout=subprocess.PIPE).stdout
    return re.search(r"NEEDED.*\[libGGPO\.so", dynamic) is not None


def run_program(program, prefix, expect):
    environment = os.environ.copy()
    launcher = environment.get("GGPO_TEST_LAUNCHER")
    if launcher:
        if expect == "shared":
            environment["GGPO_LAUNCHER_LIB_DIRS"] = str(prefix / "lib")
        else:
            environment.pop("GGPO_LAUNCHER_LIB_DIRS", None)
        run([sys.executable, launcher, program], env=environment)
        linked = needs_shared_ggpo(program)
        if linked != (expect == "shared"):
            raise SystemExit(
                f"{program.name}: expected {expect} GGPO, but libGGPO.so "
                f"{'is' if linked else 'is not'} a runtime dependency")
        return
    if WINDOWS:
        sdk_bin = str((prefix / "bin").resolve()).lower()
        paths = [part for part in environment.get("PATH", "").split(os.pathsep)
                 if part and str(Path(part).resolve()).lower() != sdk_bin]
        if expect == "shared":
            paths.insert(0, str(prefix / "bin"))
        environment["PATH"] = os.pathsep.join(paths)
    elif expect == "shared":
        environment["LD_LIBRARY_PATH"] = str(prefix / "lib")
    else:
        environment.pop("LD_LIBRARY_PATH", None)
    run([program], env=environment)
    if not WINDOWS:
        linked = needs_shared_ggpo(program)
        if linked != (expect == "shared"):
            raise SystemExit(
                f"{program.name}: expected {expect} GGPO, but libGGPO.so "
                f"{'is' if linked else 'is not'} a runtime dependency")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--source", type=Path,
                        default=Path(__file__).resolve().parent)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--generator")
    parser.add_argument("--architecture", help="Visual Studio -A value")
    parser.add_argument("--cc")
    parser.add_argument("--cxx")
    parser.add_argument("--cmake-arg", action="append", default=[])
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--build-only", action="store_true",
                        help="build and check NEEDED without running (cross ABIs "
                             "that cannot execute on this host)")
    args = parser.parse_args()

    prefix = args.prefix.resolve()
    flavors = installed_flavors(prefix)
    results = []
    for linkage, expect in cases_for(flavors):
        build = args.work.resolve() / linkage
        shutil.rmtree(build, ignore_errors=True)
        configure = ["cmake", "-S", args.source, "-B", build,
                     f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}",
                     # Cross toolchains (Android NDK) re-root package searches;
                     # an explicit GGPO_DIR works for every toolchain.
                     f"-DGGPO_DIR={(prefix / 'lib/cmake/GGPO').as_posix()}",
                     f"-DGGPO_CONSUMER_LINKAGE={linkage}",
                     f"-DGGPO_EXPECT_LINKAGE={expect}",
                     f"-DCMAKE_BUILD_TYPE={args.config}"]
        if args.generator:
            configure += ["-G", args.generator]
        if args.architecture:
            configure += ["-A", args.architecture]
        if args.cc:
            configure.append(f"-DCMAKE_C_COMPILER={args.cc}")
        if args.cxx:
            configure.append(f"-DCMAKE_CXX_COMPILER={args.cxx}")
        configure += args.cmake_arg
        run(configure)
        run(["cmake", "--build", build, "--config", args.config,
             "--parallel", str(args.jobs)])
        for name in PROGRAMS:
            program = executable(build, args.config, name)
            if args.build_only:
                linked = needs_shared_ggpo(program)
                if linked != (expect == "shared"):
                    raise SystemExit(f"{program.name}: expected {expect} GGPO, but "
                                     f"libGGPO.so {'is' if linked else 'is not'} NEEDED")
            else:
                run_program(program, prefix, expect)
        results.append({"case": linkage, "linkage": expect,
                        "programs": list(PROGRAMS), "ran": not args.build_only})
        print(f"consumer case {linkage}: {expect} linkage verified", flush=True)

    report = {"schema": 1, "installed": flavors, "cases": results}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n",
                               encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"consumer verification failed: {error}", file=sys.stderr)
        raise SystemExit(1)
