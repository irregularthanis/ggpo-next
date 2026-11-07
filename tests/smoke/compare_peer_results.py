#!/usr/bin/env python3
"""Validate result files produced by peers running on two native hosts."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "e2e"))
from result_schema import load_result, validate_pair


# Confirm two native peers reached the same deterministic session state.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("peer_a", type=Path)
    parser.add_argument("peer_b", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--scenario")
    parser.add_argument("--run-id")
    parser.add_argument("--session-id")
    parser.add_argument("--platforms", nargs=2, metavar=("PLAYER1", "PLAYER2"))
    args = parser.parse_args()

    states = [load_result(path) for path in (args.peer_a, args.peer_b)]
    try:
        report = validate_pair(
            states[0], states[1], args.frames,
            scenario=args.scenario,
            run_id=args.run_id,
            session_id=args.session_id,
            platforms=args.platforms,
        )
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
