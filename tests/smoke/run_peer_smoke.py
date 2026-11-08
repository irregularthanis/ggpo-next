#!/usr/bin/env python3
"""Run two real GGPO peers over loopback and compare their state.

Each run reserves two ephemeral ports, starts one peer process per player, and
requires both result files to report the requested frame, equal checksum, and
one running event. Repetition measures same-build timing noise; it does not
simulate Internet conditions or define a performance threshold.
"""

import argparse
import json
import os
import socket
import statistics
import subprocess
import tempfile
import time
from pathlib import Path


# Reserve an available loopback UDP port for one peer process.
def reserve_udp_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


# Run one peer pair and validate its frames, events, and final checksum.
def run_pair(executable_a, executable_b, frames, run_number,
             timeout_seconds=45, fault_a=None, fault_b=None):
    port_a = reserve_udp_port()
    port_b = reserve_udp_port()
    while port_b == port_a:
        port_b = reserve_udp_port()

    with tempfile.TemporaryDirectory(prefix="ggpo-peer-smoke-") as directory:
        result_a = Path(directory) / "peer-a.json"
        result_b = Path(directory) / "peer-b.json"
        commands = [
            [str(executable_a), "1", str(port_a), str(port_b), str(frames), str(result_a)],
            [str(executable_b), "2", str(port_b), str(port_a), str(frames), str(result_b)],
        ]
        started = time.perf_counter()
        environments = []
        for fault in (fault_a, fault_b):
            environment = os.environ.copy()
            if fault:
                environment["GGPO_SMOKE_FAULT"] = fault
            else:
                environment.pop("GGPO_SMOKE_FAULT", None)
            environments.append(environment)
        processes = [
            subprocess.Popen(command, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True,
                             env=environments[index])
            for index, command in enumerate(commands)
        ]
        outputs = []
        try:
            for process in processes:
                output, _ = process.communicate(timeout=timeout_seconds)
                outputs.append(output)
        except subprocess.TimeoutExpired:
            for process in processes:
                process.kill()
            for process in processes:
                process.wait()
            raise RuntimeError("peer smoke timed out")

        elapsed_ms = (time.perf_counter() - started) * 1000.0
        # Emit both peers before evaluating either return code so a failure on
        # one side cannot hide the diagnostic that explains it on the other.
        for output in outputs:
            print(output, end="")
        for index, process in enumerate(processes):
            if process.returncode != 0:
                raise RuntimeError(
                    f"peer {index + 1} exited with {process.returncode}: "
                    f"{outputs[index].strip()}"
                )

        states = [
            json.loads(result_a.read_text(encoding="utf-8")),
            json.loads(result_b.read_text(encoding="utf-8")),
        ]
        if states[0]["frame"] != frames or states[1]["frame"] != frames:
            raise RuntimeError(f"incomplete frames: {states}")
        if states[0]["checksum"] != states[1]["checksum"]:
            raise RuntimeError(f"peer checksum mismatch: {states}")
        if any(state["running_events"] != 1 for state in states):
            raise RuntimeError(f"unexpected running events: {states}")

        print(
            f"peer-pair run={run_number} frames={frames} "
            f"checksum={states[0]['checksum']:08x} elapsed_ms={elapsed_ms:.3f}"
        )
        return {
            "run": run_number,
            "frames": frames,
            "checksum": states[0]["checksum"],
            "elapsed_ms": elapsed_ms,
            "peer_a": states[0],
            "peer_b": states[1],
        }


# Repeat the peer test and summarize its observed execution time.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--peer-b", type=Path,
                        help="optional second peer executable")
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args()

    executable_a = args.executable.resolve()
    executable_b = (args.peer_b or args.executable).resolve()
    results = [
        run_pair(executable_a, executable_b, args.frames, run_number + 1)
        for run_number in range(args.runs)
    ]
    elapsed = [result["elapsed_ms"] for result in results]
    summary = {
        "schema": 1,
        "executables": [executable_a.name, executable_b.name],
        "runs": results,
        "elapsed_ms": {
            "minimum": min(elapsed),
            "median": statistics.median(elapsed),
            "maximum": max(elapsed),
        },
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(
            json.dumps(summary, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()
