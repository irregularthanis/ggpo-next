#!/usr/bin/env python3
"""Hole punching with relay fallback on loopback: the real matchmaking server
plus one net_proxy_tool process per player.

- direct, 2 and 3 players: every player receives every other player's data and
  every pair ends on the direct path;
- relay (direct disabled): every player still receives everything through the
  relay, and no pair leaves it.
"""

import argparse
import json
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


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


def run(tool, services, players, mode, seconds=4):
    lobby_port, *relay = free_ports(1 + players)
    server = subprocess.Popen(
        [sys.executable, "-m", "matchmaking", "--host", "127.0.0.1", "--port", str(lobby_port),
         "--relay-ports", f"{min(relay)}-{min(relay) + players - 1}"],
        cwd=services, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(0.5)
    try:
        with tempfile.TemporaryDirectory(prefix="vw-proxy-") as directory:
            results = [Path(directory) / f"player{i}.json" for i in range(players)]
            tools = [subprocess.Popen([str(tool), str(lobby_port), str(players), "-", mode,
                                       str(seconds), str(result)],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                     for result in results]
            outputs = [t.communicate(timeout=60)[0] for t in tools]
            for t, output in zip(tools, outputs):
                if t.returncode != 0:
                    raise SystemExit(f"{mode}/{players}: tool failed ({t.returncode}): {output}")
            return [json.loads(r.read_text(encoding="utf-8")) for r in results]
    finally:
        server.terminate()
        server.wait(timeout=10)


def check(name, reports, players, expect_direct):
    for report in reports:
        if report["wrong_source"]:
            raise SystemExit(f"{name}: player {report['player']} got datagrams from the wrong endpoint")
        for peer in report["peers"]:
            # Data must flow throughout; allow startup and switch losses.
            if peer["received"] < report["sent"] * 0.5:
                raise SystemExit(f"{name}: player {report['player']} received only "
                                 f"{peer['received']} of ~{report['sent']} from {peer['player']}")
            if peer["direct"] != expect_direct:
                raise SystemExit(f"{name}: player {report['player']} -> {peer['player']} "
                                 f"direct={peer['direct']}, expected {expect_direct}: {report}")
    print(f"{name}: {players} players, direct={expect_direct}: "
          + "; ".join(f"P{r['player']} relay {[p['relay_rtt_ms'] for p in r['peers']]} ms, "
                      f"direct {[p['direct_rtt_ms'] for p in r['peers']]} ms" for r in reports))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("tool", type=Path)
    parser.add_argument("services", type=Path, help="services/matchmaking")
    args = parser.parse_args()
    silent_port, = free_ports(1)
    errors = subprocess.run([str(args.tool), "--client-errors", str(silent_port)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
    print(errors.stdout.strip())
    if errors.returncode != 0:
        raise SystemExit(f"client errors: tool failed ({errors.returncode})")
    check("hole punching", run(args.tool, args.services, 2, "direct"), 2, True)
    check("hole punching", run(args.tool, args.services, 3, "direct"), 3, True)
    check("relay fallback", run(args.tool, args.services, 2, "relay"), 2, False)


if __name__ == "__main__":
    main()
