#!/usr/bin/env python3
"""Validate result files produced by peers running on two native hosts."""

import argparse
import json
from pathlib import Path


# Confirm two native peers reached the same deterministic session state.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("peer_a", type=Path)
    parser.add_argument("peer_b", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    args = parser.parse_args()

    states = [json.loads(path.read_text(encoding="utf-8"))
              for path in (args.peer_a, args.peer_b)]
    if [state["player"] for state in states] != [1, 2]:
        raise SystemExit(f"unexpected players: {states}")
    if any(state["frame"] != args.frames for state in states):
        raise SystemExit(f"incomplete frames: {states}")
    if states[0]["checksum"] != states[1]["checksum"]:
        raise SystemExit(f"peer checksum mismatch: {states}")
    if any(state["running_events"] != 1 for state in states):
        raise SystemExit(f"unexpected running events: {states}")
    print(json.dumps({
        "frames": args.frames,
        "checksum": states[0]["checksum"],
        "peers": states,
        "result": "pass",
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
