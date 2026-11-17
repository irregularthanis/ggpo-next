#!/usr/bin/env python3
"""Build and test GGPO with the repository Linux CI image."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


# Run a command visibly and stop the build on failure.
def run(command, **kwargs):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


# Validate the host, then build and verify one Linux SDK configuration.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", choices=("gcc", "clang"), default="gcc")
    parser.add_argument("--library", choices=("both", "static", "shared"),
                        default="both",
                        help="both installs libGGPO.so and libGGPO.a together")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--image", default="ggpo-linux-ci:local")
    parser.add_argument("--skip-image-build", action="store_true")
    parser.add_argument("--packages", action="store_true",
                        help="create and verify raw, DEB, and RPM SDK packages")
    parser.add_argument("--sanitizers", action="store_true",
                        help="instrument GGPO and tests with ASan and UBSan")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args()

    # An instrumented libGGPO.so needs the sanitizers' C++ runtime in every
    # C consumer link; the qualification target is the static library.
    if args.sanitizers and args.library != "static":
        parser.error("--sanitizers requires --library static")

    if shutil.which("docker") is None:
        parser.error("Docker is required but was not found in PATH")
    try:
        subprocess.run(
            ["docker", "info"], check=True, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        parser.error("Docker is installed, but its daemon is unavailable")

    repo = Path(__file__).resolve().parents[2]
    output = (args.out or repo / "build" / f"linux-{args.compiler}-{args.library}")
    output = output.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)

    if not args.skip_image_build:
        run([
            "docker", "build", "-f", repo / "docker/linux/Dockerfile",
            "-t", args.image, repo,
        ])

    cc, cxx = (("gcc", "g++") if args.compiler == "gcc"
               else ("clang", "clang++"))
    linkage_flag = {
        "both": "",
        "shared": "-DBUILD_SHARED_LIBS=ON ",
        "static": "-DBUILD_SHARED_LIBS=OFF ",
    }[args.library]
    sanitizer_flags = ""
    flags = "-fsanitize=address,undefined -fno-omit-frame-pointer"
    if args.sanitizers:
        sanitizer_flags = (
            f"-DCMAKE_C_FLAGS='{flags}' -DCMAKE_CXX_FLAGS='{flags}' "
            f"-DCMAKE_EXE_LINKER_FLAGS='{flags}' "
            f"-DCMAKE_SHARED_LINKER_FLAGS='{flags}' "
        )
    commands = [
        f"cmake -S /src -B /out/build -G Ninja "
        f"-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx} "
        f"-DCMAKE_BUILD_TYPE=Release {linkage_flag}"
        f"{sanitizer_flags}"
        "-DGGPO_BUILD_SDK=ON -DGGPO_BUILD_VECTORWAR=OFF "
        "-DGGPO_RUN_TESTS_ON_BUILD=ON -DBUILD_TESTING=ON "
        "-DCMAKE_INSTALL_PREFIX=/out/install",
        f"cmake --build /out/build --parallel {args.jobs}",
        "ctest --test-dir /out/build --output-on-failure",
        "cmake --install /out/build",
        # Every installed linkage: default, GGPO_USE_STATIC=ON, GGPO::shared.
        f"python3 /src/tests/consumer/verify_consumers.py "
        f"--prefix /out/install --work /out/consumer "
        f"--generator Ninja --cc {cc} --cxx {cxx} --jobs {args.jobs} "
        f"--report /out/consumer/consumers.json"
        + (f" --cmake-arg=-DCMAKE_C_FLAGS='{flags}' "
           f"--cmake-arg=-DCMAKE_CXX_FLAGS='{flags}' "
           f"--cmake-arg=-DCMAKE_EXE_LINKER_FLAGS='{flags}'"
           if args.sanitizers else ""),
    ]
    if not args.sanitizers:
        commands.append("python3 /src/scripts/ci/audit_linux_sdk.py /out/install")
        # The Vector War SDL frontend and its host contract (legend, start
        # screen) under SDL's dummy video driver; the SDK build above leaves
        # Vector War off.
        commands.extend([
            f"cmake -S /src -B /out/vectorwar -G Ninja "
            f"-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx} "
            "-DCMAKE_BUILD_TYPE=Release -DGGPO_BUILD_VECTORWAR=ON "
            "-DGGPO_RUN_TESTS_ON_BUILD=OFF -DBUILD_TESTING=ON",
            f"cmake --build /out/vectorwar --parallel {args.jobs}",
            "ctest --test-dir /out/vectorwar --output-on-failure "
            "-R 'gui_host_contract|legend_contract|start_form_contract'",
        ])
    commands.extend([
        f"{cc} --version | head -n 1",
        "cmake --version | head -n 1",
        "ninja --version",
    ])
    if args.packages:
        commands.append(
            "python3 /src/scripts/ci/package_linux_sdk.py "
            "--build /out/build --install /out/install "
            "--out /out/packages --source /src "
            f"--cc {cc} --cxx {cxx} --jobs {args.jobs}"
        )
    container_command = " && ".join(commands)
    docker_command = [
        "docker", "run", "--rm",
        "--user", f"{os.getuid()}:{os.getgid()}",
        "--env", "HOME=/tmp",
    ]
    if args.sanitizers:
        docker_command.extend([
            "--env", "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1",
            "--env", "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1",
        ])
    docker_command.extend([
        "--volume", f"{repo}:/src:ro",
        "--volume", f"{output}:/out",
        "--workdir", "/src",
        args.image, "sh", "-euxc", container_command,
    ])
    run(docker_command)
    print(f"Linux build output: {output}")
    print(f"Installed SDK: {output / 'install'}")
    if args.packages:
        print(f"Verified packages: {output / 'packages'}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"Linux container build failed with exit code {error.returncode}",
              file=sys.stderr)
        raise SystemExit(error.returncode)
