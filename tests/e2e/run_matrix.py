#!/usr/bin/env python3
"""Generate the complete native platform/role matrix for E2E execution."""

import argparse
import json


def generate_matrix(platforms=("linux", "windows")):
    """Return every ordered player-1/player-2 assignment exactly once."""
    return [
        {"scenario": f"{left}-{right}", "player1": left, "player2": right}
        for left in platforms for right in platforms
    ]


def validate_matrix(matrix, platforms=("linux", "windows")):
    expected = {(left, right) for left in platforms for right in platforms}
    actual = [(item["player1"], item["player2"]) for item in matrix]
    if len(actual) != len(set(actual)):
        raise ValueError("matrix contains duplicate role assignments")
    if set(actual) != expected:
        raise ValueError("matrix is missing or adds role assignments")
    return matrix


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compact", action="store_true")
    args = parser.parse_args()
    matrix = validate_matrix(generate_matrix())
    print(json.dumps(matrix, separators=(",", ":") if args.compact else None,
                     indent=None if args.compact else 2))


if __name__ == "__main__":
    main()
