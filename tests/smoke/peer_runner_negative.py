#!/usr/bin/env python3
"""Prove the peer runner rejects controlled lifecycle and state failures."""

import argparse
from pathlib import Path

from run_peer_smoke import run_pair


# Require one injected fault to fail with the expected diagnostic.
def expect_failure(name, executable, expected, timeout_seconds=15, **faults):
    try:
        run_pair(
            executable, executable, 1, 1,
            timeout_seconds=timeout_seconds, **faults
        )
    except RuntimeError as error:
        if expected not in str(error):
            raise RuntimeError(
                f"{name} produced the wrong diagnostic: {error}"
            )
        print(f"negative-case: {name}: pass ({error})")
        return
    raise RuntimeError(f"{name} was not detected")


# Exercise every failure family required by the interoperability plan.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()

    expect_failure(
        "startup", executable, "fault: startup",
        timeout_seconds=2,
        fault_a="startup", fault_b="startup",
    )
    expect_failure(
        "timeout", executable, "peer smoke timed out",
        timeout_seconds=2,
        fault_a="timeout", fault_b="timeout",
    )
    expect_failure(
        "state mismatch", executable, "peer checksum mismatch",
        fault_a="mismatch",
    )
    expect_failure(
        "shutdown", executable, "fault: shutdown",
        fault_a="shutdown", fault_b="shutdown",
    )


if __name__ == "__main__":
    main()
