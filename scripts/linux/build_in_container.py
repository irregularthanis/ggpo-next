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
    parser.add_argument("--library", choices=("static", "shared"), default="shared")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--image", default="ggpo-linux-ci:local")
    parser.add_argument("--skip-image-build", action="store_true")
    parser.add_argument("--packages", action="store_true",
                        help="create and verify raw, DEB, and RPM SDK packages")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args()

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
    shared = "ON" if args.library == "shared" else "OFF"
    commands = [
        f"cmake -S /src -B /out/build -G Ninja "
        f"-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx} "
        f"-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS={shared} "
        "-DGGPO_BUILD_SDK=ON -DGGPO_BUILD_VECTORWAR=OFF "
        "-DGGPO_RUN_TESTS_ON_BUILD=ON -DBUILD_TESTING=ON "
        "-DCMAKE_INSTALL_PREFIX=/out/install",
        f"cmake --build /out/build --parallel {args.jobs}",
        "ctest --test-dir /out/build --output-on-failure",
        "cmake --install /out/build",
        f"cmake -S /src/tests/consumer -B /out/consumer -G Ninja "
        f"-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx} "
        "-DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/out/install",
        f"cmake --build /out/consumer --parallel {args.jobs}",
        "LD_LIBRARY_PATH=/out/install/lib /out/consumer/ggpo_installed_c11",
        "LD_LIBRARY_PATH=/out/install/lib /out/consumer/ggpo_installed_cpp98",
        "python3 /src/scripts/ci/audit_linux_sdk.py /out/install",
        f"{cc} --version | head -n 1",
        "cmake --version | head -n 1",
        "ninja --version",
    ]
    if args.packages:
        commands.append(
            "python3 /src/scripts/ci/package_linux_sdk.py "
            "--build /out/build --install /out/install "
            "--out /out/packages --source /src "
            f"--cc {cc} --cxx {cxx} --jobs {args.jobs}"
        )
    container_command = " && ".join(commands)
    run([
        "docker", "run", "--rm",
        "--user", f"{os.getuid()}:{os.getgid()}",
        "--env", "HOME=/tmp",
        "--volume", f"{repo}:/src:ro",
        "--volume", f"{output}:/out",
        "--workdir", "/src",
        args.image, "sh", "-euxc", container_command,
    ])
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
