#!/usr/bin/env python3
"""Real Vector War sessions through matchmaking, hole punching and the relay.

Starts the matchmaking server on loopback and one VectorWarHeadless per player
in --matchmaking mode. Every player must reach the target frame with the same
deterministic state; two players must reach Vector War's frozen checksum, the
same one direct play and the E2E matrix use. With hole punching every pair
must end on the direct path; with --relay-only every pair stays on the relay.
"""

import argparse
import json
import re
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

FROZEN_TWO_PLAYER_CHECKSUM = 0xB3381958
PATH_LINE = re.compile(r"vectorwar-path: player=(\d+) peer=(\d+) route=(\w+)")


def free_ports(count):
    sockets = []
    try:
        for _ in range(count):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(("127.0.0.1", 0))
            sockets.append(s)
        return [s.getsockname()[1] for s in sockets]
    finally:
        for s in sockets:
            s.close()


def session(headless, services, players, relay_only, frames=180):
    lobby_port, *relay = free_ports(1 + players)
    first = min(relay)
    server = subprocess.Popen(
        [sys.executable, "-m", "matchmaking", "--host", "127.0.0.1", "--port", str(lobby_port),
         "--relay-ports", f"{first}-{first + players - 1}"],
        cwd=services, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(0.5)
    try:
        with tempfile.TemporaryDirectory(prefix="vw-mm-") as directory:
            results = [Path(directory) / f"player{i}.json" for i in range(players)]
            command = lambda result: ([str(headless), "--matchmaking", f"127.0.0.1:{lobby_port}",
                                       str(players), "-", str(frames), str(result)]
                                      + (["--relay-only"] if relay_only else []))
            processes = [subprocess.Popen(command(result), stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT, text=True)
                         for result in results]
            outputs = [p.communicate(timeout=120)[0] for p in processes]
            for process, output in zip(processes, outputs):
                print(output, end="")
                if process.returncode != 0:
                    raise SystemExit(f"{players} players: a session failed ({process.returncode})")
            reports = [json.loads(r.read_text(encoding="utf-8")) for r in results]
            routes = [m.groups() for output in outputs for m in PATH_LINE.finditer(output)]
            return reports, routes
    finally:
        server.terminate()
        server.wait(timeout=10)


def check(name, reports, routes, players, expect_route):
    frames = {r["frame"] for r in reports}
    checksums = {r["checksum"] for r in reports}
    if frames != {180}:
        raise SystemExit(f"{name}: frames {frames}")
    if len(checksums) != 1:
        raise SystemExit(f"{name}: peers diverged: {checksums}")
    checksum = checksums.pop()
    if players == 2 and checksum != FROZEN_TWO_PLAYER_CHECKSUM:
        raise SystemExit(f"{name}: checksum {checksum:08x}, expected frozen {FROZEN_TWO_PLAYER_CHECKSUM:08x}")
    if len(routes) != players * (players - 1):
        raise SystemExit(f"{name}: expected {players * (players - 1)} route lines, got {routes}")
    wrong = [r for r in routes if r[2] != expect_route]
    if wrong:
        raise SystemExit(f"{name}: expected every pair on {expect_route}, got {wrong}")
    print(f"PASS {name}: {players} players, checksum {checksum:08x}, every pair {expect_route}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("headless", type=Path)
    parser.add_argument("services", type=Path, help="services/matchmaking")
    args = parser.parse_args()
    for players in (2, 3, 4):
        check("hole punching", *session(args.headless, args.services, players, False), players, "direct")
    check("relay fallback", *session(args.headless, args.services, 2, True), 2, "relay")


if __name__ == "__main__":
    main()
