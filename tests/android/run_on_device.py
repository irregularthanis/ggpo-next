#!/usr/bin/env python3
"""Run one host-built Android executable on a device or emulator through adb.

Used as CMAKE_CROSSCOMPILING_EMULATOR and as GGPO_TEST_LAUNCHER for the Python
peer runners, so the regular test suite runs unchanged on Android:

- pushes the executable and every shared library beside it (libGGPO.so) into a
  fresh per-run directory under /data/local/tmp;
- rewrites absolute host-path arguments and GGPO_*_FILE variables to files in
  that directory and keeps them in sync both ways while the program runs:
  files created on the device (ready, result) are pulled, files created on the
  host (the coordinator's finalize file) are pushed;
- forwards GGPO_* environment variables (faults, run identity);
- streams output and returns the program's exit code. Devices below API 24 do
  not report exit codes through `adb shell`, so the code is read from a marker.

Optional ANDROID_SERIAL selects the device, as with adb itself.
"""

import os
import shlex
import signal
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

ROOT = os.environ.get("GGPO_DEVICE_ROOT", "/data/local/tmp/ggpo")
MARKER = "__GGPO_EXIT__"


def adb(*arguments, check=True, **kwargs):
    return subprocess.run(["adb", *arguments], check=check, **kwargs)


def main():
    if len(sys.argv) < 2:
        print("usage: run_on_device.py <executable> [arguments...]", file=sys.stderr)
        return 2
    executable = Path(sys.argv[1]).resolve()
    device_dir = f"{ROOT}/{uuid.uuid4().hex}"
    adb("shell", f"mkdir -p {device_dir}", stdout=subprocess.DEVNULL)

    adb("push", str(executable), f"{device_dir}/{executable.name}",
        stdout=subprocess.DEVNULL)
    # Shared libraries beside the executable, plus any directories listed in
    # GGPO_LAUNCHER_LIB_DIRS (an installed SDK's lib/ for consumer checks).
    library_dirs = [executable.parent] + [
        Path(entry) for entry in
        os.environ.get("GGPO_LAUNCHER_LIB_DIRS", "").split(os.pathsep) if entry]
    for directory in library_dirs:
        for library in directory.glob("*.so"):
            adb("push", str(library), f"{device_dir}/{library.name}",
                stdout=subprocess.DEVNULL)

    mapped = {}
    arguments = []
    for argument in sys.argv[2:]:
        path = Path(argument)
        if path.is_absolute() and path.parent.exists():
            device_path = f"{device_dir}/{path.name}"
            if path.is_file():
                adb("push", str(path), device_path, stdout=subprocess.DEVNULL)
            mapped[device_path] = path
            arguments.append(device_path)
        else:
            arguments.append(argument)

    forwarded = {}
    for name, value in os.environ.items():
        if not name.startswith("GGPO_") or name in ("GGPO_TEST_LAUNCHER", "GGPO_DEVICE_ROOT",
                                                         "GGPO_LAUNCHER_LIB_DIRS"):
            continue
        path = Path(value)
        if name.endswith("_FILE") and path.is_absolute():
            device_path = f"{device_dir}/{path.name}"
            mapped[device_path] = path
            value = device_path
        forwarded[name] = value
    environment = " ".join(f"{name}={shlex.quote(value)}"
                           for name, value in forwarded.items())
    command = (f"cd {device_dir} && chmod 755 {executable.name} && "
               f"LD_LIBRARY_PATH={device_dir} {environment} "
               f"./{executable.name} {' '.join(shlex.quote(a) for a in arguments)}; "
               f"echo {MARKER}$?")

    process = subprocess.Popen(["adb", "shell", command], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)

    def device_exists(device_path):
        return adb("shell", f"test -e {device_path} && echo yes", check=False,
                   capture_output=True, text=True).stdout.strip() == "yes"

    # A file's direction is fixed the first time it is seen: created on the
    # device -> pulled every cycle (peers rewrite their ready state until the
    # corrected state settles); created on the host -> pushed, and re-pushed
    # when it changes (the coordinator's finalize file).
    direction = {}
    pushed_mtime = {}

    def sync_once():
        for device_path, host_path in mapped.items():
            mode = direction.get(device_path)
            if mode is None:
                if device_exists(device_path):
                    mode = direction[device_path] = "pull"
                elif host_path.exists():
                    mode = direction[device_path] = "push"
                else:
                    continue
            if mode == "pull":
                adb("pull", device_path, str(host_path), check=False,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            else:
                mtime = host_path.stat().st_mtime if host_path.exists() else None
                if mtime is not None and pushed_mtime.get(device_path) != mtime:
                    adb("push", str(host_path), device_path, check=False,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    pushed_mtime[device_path] = mtime

    stop = threading.Event()

    def sync_loop():
        while not stop.is_set():
            sync_once()
            stop.wait(0.25)

    syncer = threading.Thread(target=sync_loop, daemon=True)
    if mapped:
        syncer.start()

    def terminate(signum, frame):
        adb("shell", f"pkill -f {device_dir} ; true", check=False,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        process.kill()
        sys.exit(128 + signum)

    signal.signal(signal.SIGTERM, terminate)
    signal.signal(signal.SIGINT, terminate)

    code = 1
    for line in process.stdout:
        line = line.rstrip("\r\n")
        if line.startswith(MARKER):
            code = int(line[len(MARKER):] or 1)
            continue
        print(line, flush=True)
    process.wait()
    stop.set()
    if mapped:
        syncer.join()

    # Final state: everything the program wrote replaces the host copy.
    for device_path, host_path in mapped.items():
        if direction.get(device_path) == "push":
            continue
        adb("pull", device_path, str(host_path), check=False,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    adb("shell", f"rm -rf {device_dir}", check=False, stdout=subprocess.DEVNULL)
    return code


if __name__ == "__main__":
    sys.exit(main())
