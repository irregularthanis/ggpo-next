#!/usr/bin/env python3
"""Run the real Vector War core as two peers and enforce its golden oracle."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "smoke"))
from run_peer_smoke import run_pair


EXPECTED_FRAME_180_CHECKSUM = 0xB3381958


def run_vectorwar_pair(executable, fault_a=None, fault_b=None,
                       timeout_seconds=45):
    result = run_pair(executable, executable, 180, 1,
                      fault_a=fault_a, fault_b=fault_b,
                      timeout_seconds=timeout_seconds)
    if result["checksum"] != EXPECTED_FRAME_180_CHECKSUM:
        raise RuntimeError(
            "Vector War golden checksum mismatch: "
            f"expected {EXPECTED_FRAME_180_CHECKSUM:08x}, "
            f"got {result['checksum']:08x}"
        )
    replay_frames = (
        result["peer_a"]["replay_frames"] +
        result["peer_b"]["replay_frames"]
    )
    if replay_frames <= 0:
        raise RuntimeError("Vector War fixture completed without rollback")
    print(
        "vectorwar-oracle: "
        f"checksum={result['checksum']:08x} replay_frames={replay_frames}"
    )
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    run_vectorwar_pair(args.executable.resolve())


if __name__ == "__main__":
    main()
