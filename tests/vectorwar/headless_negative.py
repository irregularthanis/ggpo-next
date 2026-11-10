#!/usr/bin/env python3
"""Prove the Vector War application runner rejects controlled failures."""

import argparse
from pathlib import Path

from run_headless_loopback import run_vectorwar_pair


def expect_failure(name, executable, expected, timeout_seconds=15):
    try:
        run_vectorwar_pair(executable, name, name, timeout_seconds)
    except RuntimeError as error:
        if expected not in str(error):
            raise RuntimeError(
                f"{name} produced the wrong diagnostic: {error}"
            )
        print(f"vectorwar-negative: {name}: pass ({error})")
        return
    raise RuntimeError(f"{name} was not detected")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()

    expect_failure("startup", executable, "fault: startup", 2)
    expect_failure("timeout", executable, "peer smoke timed out", 2)
    expect_failure("mismatch", executable, "peer checksum mismatch")
    expect_failure("shutdown", executable, "fault: shutdown")
    expect_failure("no_rollback", executable, "completed without rollback")


if __name__ == "__main__":
    main()
