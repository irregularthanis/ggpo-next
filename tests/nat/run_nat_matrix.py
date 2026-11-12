#!/usr/bin/env python3
"""Hole punching with relay fallback through emulated NAT routers.

Builds a small Internet out of Linux network namespaces (root required; CI
only): a bridge on 198.18.0.0/24, the matchmaking server at 198.18.0.1, and one
router namespace per site with a private 10.N.0.0/24 LAN and NAT on its WAN:

- cone:      MASQUERADE; one public port per socket, replies only (a typical
             home router: port-restricted cone).
- symmetric: MASQUERADE --random-fully; a different public port per
             destination (carrier-grade or strict NAT).

Each player is a namespace on a router's LAN running
`VectorWarHeadless --matchmaking`. Every scenario must converge; each also
checks the route every pair ended on, and the mid-match scenarios check the
switch and fallback counters.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PREFIX = "vwn"
SERVER_IP = "198.18.0.1"
LOBBY_PORT = 7100
RELAY_PORTS = "7101-7120"
FROZEN_TWO_PLAYER_CHECKSUM = 0xB3381958
MATCH_LINE = re.compile(r"vectorwar-match: player=(\d+) of (\d+)")
PATH_LINE = re.compile(r"vectorwar-path: player=(\d+) peer=(\d+) route=(\w+) via=([\d.]+):\d+ "
                       r"relay_rtt_ms=(-?\d+) direct_rtt_ms=(-?\d+) switches=(\d+) fallbacks=(\d+)")


def run(*command, check=True):
    return subprocess.run([str(c) for c in command], check=check, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def ns(name, *command, check=True):
    return run("ip", "netns", "exec", name, *command, check=check)


class Network:
    """Namespaces for one scenario; removed again by close()."""

    def __init__(self):
        self.namespaces = []
        self.routers = []        # (namespace, wan ip, kind)

    def add_ns(self, name):
        run("ip", "netns", "add", name)
        ns(name, "ip", "link", "set", "lo", "up")
        self.namespaces.append(name)
        return name

    def veth(self, a_ns, a_name, b_ns, b_name):
        # Create with unique temporary names (the root namespace already has
        # interfaces such as eth0), then rename inside each namespace.
        self.serial = getattr(self, "serial", 0) + 1
        tmp_a, tmp_b = f"vwa{self.serial}", f"vwb{self.serial}"
        run("ip", "link", "add", tmp_a, "type", "veth", "peer", "name", tmp_b)
        run("ip", "link", "set", tmp_a, "netns", a_ns)
        run("ip", "link", "set", tmp_b, "netns", b_ns)
        ns(a_ns, "ip", "link", "set", "dev", tmp_a, "name", a_name)
        ns(b_ns, "ip", "link", "set", "dev", tmp_b, "name", b_name)

    def build_internet(self):
        self.wan = self.add_ns(f"{PREFIX}-wan")
        ns(self.wan, "ip", "link", "add", "br0", "type", "bridge")
        ns(self.wan, "ip", "link", "set", "br0", "up")
        self.server = self.add_ns(f"{PREFIX}-srv")
        self.veth(self.server, "eth0", self.wan, "srv")
        ns(self.wan, "ip", "link", "set", "srv", "master", "br0", "up")
        ns(self.server, "ip", "addr", "add", f"{SERVER_IP}/24", "dev", "eth0")
        ns(self.server, "ip", "link", "set", "eth0", "up")

    def add_router(self, index, kind):
        router = self.add_ns(f"{PREFIX}-r{index}")
        wan_ip = f"198.18.0.{10 + index}"
        self.veth(router, "wan0", self.wan, f"r{index}")
        ns(self.wan, "ip", "link", "set", f"r{index}", "master", "br0", "up")
        ns(router, "ip", "addr", "add", f"{wan_ip}/24", "dev", "wan0")
        ns(router, "ip", "link", "set", "wan0", "up")
        ns(router, "ip", "link", "add", "lan", "type", "bridge")
        ns(router, "ip", "addr", "add", f"10.{index}.0.1/24", "dev", "lan")
        ns(router, "ip", "link", "set", "lan", "up")
        ns(router, "sysctl", "-qw", "net.ipv4.ip_forward=1")
        masquerade = ["iptables", "-t", "nat", "-A", "POSTROUTING", "-o", "wan0", "-j", "MASQUERADE"]
        if kind == "symmetric":
            masquerade.append("--random-fully")
        ns(router, *masquerade)
        # Drop unsolicited inbound UDP before conntrack confirms it, as home
        # routers do. Otherwise a peer's early probe leaves a confirmed entry
        # that makes MASQUERADE pick another source port for the later
        # outbound flow, so cone NAT would never punch through.
        ns(router, "iptables", "-A", "INPUT", "-i", "wan0", "-p", "udp",
           "-m", "conntrack", "--ctstate", "NEW", "-j", "DROP")
        self.routers.append((router, wan_ip, kind))
        return router

    def add_player(self, router_index, slot):
        router = f"{PREFIX}-r{router_index}"
        player = self.add_ns(f"{PREFIX}-p{router_index}{slot}")
        self.veth(player, "eth0", router, f"p{slot}")
        ns(router, "ip", "link", "set", f"p{slot}", "master", "lan", "up")
        ns(player, "ip", "addr", "add", f"10.{router_index}.0.{1 + slot}/24", "dev", "eth0")
        ns(player, "ip", "link", "set", "eth0", "up")
        ns(player, "ip", "route", "add", "default", "via", f"10.{router_index}.0.1")
        return player

    def cut_between_routers(self):
        """Drop all traffic between the routers' WAN addresses (the direct
        paths); traffic to and from the server is untouched."""
        for router, own_ip, _ in self.routers:
            for _, other_ip, _ in self.routers:
                if other_ip == own_ip:
                    continue
                ns(router, "iptables", "-I", "FORWARD", "-s", other_ip, "-j", "DROP")
                ns(router, "iptables", "-I", "FORWARD", "-d", other_ip, "-j", "DROP")

    def close(self):
        for name in reversed(self.namespaces):
            for pid in run("ip", "netns", "pids", name, check=False).stdout.split():
                run("kill", "-9", pid, check=False)
            run("ip", "netns", "del", name, check=False)


def play(scenario, headless, services):
    """Run one scenario; returns per-player reports with routes."""
    network = Network()
    server = None
    try:
        network.build_internet()
        players = []                                   # (namespace, router index)
        for index, (kind, count) in enumerate(scenario["sites"], start=1):
            network.add_router(index, kind)
            for slot in range(1, count + 1):
                players.append((network.add_player(index, slot), index))
        server = subprocess.Popen(
            ["ip", "netns", "exec", network.server, sys.executable, "-m", "matchmaking",
             "--host", "0.0.0.0", "--port", str(LOBBY_PORT), "--relay-ports", RELAY_PORTS],
            cwd=services, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(1.0)
        # Timed events need a real-time match: 1200 frames at 16 ms is ~19 s.
        paced_env = dict(os.environ, VW_HEADLESS_FRAME_MS="16")
        with tempfile.TemporaryDirectory(prefix="vw-nat-") as directory:
            results = [Path(directory) / f"{i}.json" for i in range(len(players))]
            processes = [subprocess.Popen(
                ["ip", "netns", "exec", player, str(headless), "--matchmaking",
                 f"{SERVER_IP}:{LOBBY_PORT}", str(len(players)), scenario["lobby"],
                 str(scenario["frames"]), str(result)],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                env=paced_env if scenario.get("event") else None)
                for (player, _), result in zip(players, results)]
            event = scenario.get("event")
            if event:
                time.sleep(scenario["event_after_s"])
                if event == "cut-direct":
                    network.cut_between_routers()
                elif event == "kill-server":
                    server.kill()
                print(f"  event: {event} at {scenario['event_after_s']} s")
            outputs = [p.communicate(timeout=180)[0] for p in processes]
            reports = []
            for (player_ns, site), process, output, result in zip(players, processes, outputs, results):
                if process.returncode != 0:
                    raise SystemExit(f"{scenario['name']}: {player_ns} failed "
                                     f"({process.returncode}):\n{output}")
                report = json.loads(result.read_text(encoding="utf-8"))
                match = MATCH_LINE.search(output)
                report["site"] = site
                report["kind"] = scenario["sites"][site - 1][0]
                report["player_id"] = int(match.group(1)) if match else None
                report["routes"] = [
                    {"peer": int(m.group(2)), "route": m.group(3), "via": m.group(4),
                     "relay_rtt_ms": int(m.group(5)), "direct_rtt_ms": int(m.group(6)),
                     "switches": int(m.group(7)), "fallbacks": int(m.group(8))}
                    for m in PATH_LINE.finditer(output)]
                reports.append(report)
            return reports
    finally:
        if server and server.poll() is None:
            server.terminate()
        network.close()


def common_checks(scenario, reports):
    name = scenario["name"]
    if {r["frame"] for r in reports} != {scenario["frames"]}:
        raise SystemExit(f"{name}: frames {[r['frame'] for r in reports]}")
    checksums = {r["checksum"] for r in reports}
    if len(checksums) != 1:
        raise SystemExit(f"{name}: peers diverged: {checksums}")
    if len(reports) == 2 and scenario["frames"] == 180 and checksums != {FROZEN_TWO_PLAYER_CHECKSUM}:
        raise SystemExit(f"{name}: checksum {checksums}, expected the frozen {FROZEN_TWO_PLAYER_CHECKSUM:08x}")
    for report in reports:
        if len(report["routes"]) != len(reports) - 1:
            raise SystemExit(f"{name}: player {report['player_id']} reported {report['routes']}")


def site_of(reports, player_id):
    return next(r["site"] for r in reports if r["player_id"] == player_id)


def all_routes(reports):
    for report in reports:
        for route in report["routes"]:
            yield report, route


def expect_all(route_name, extra=None):
    def check(scenario, reports):
        for report, route in all_routes(reports):
            if route["route"] != route_name:
                raise SystemExit(f"{scenario['name']}: P{report['player_id']}->P{route['peer']} "
                                 f"on {route['route']}, expected {route_name}: {route}")
            if extra:
                extra(scenario, report, route)
    return check


def via_lan(scenario, report, route):
    if not route["via"].startswith("10."):
        raise SystemExit(f"{scenario['name']}: expected the LAN candidate, got {route['via']}")


def direct_then_fallback(scenario, report, route):
    if route["switches"] < 1 or route["fallbacks"] < 1:
        raise SystemExit(f"{scenario['name']}: expected a switch and a fallback: {route}")


def cone_pair_direct(scenario, reports):
    for report, route in all_routes(reports):
        mine, theirs = report["site"], site_of(reports, route["peer"])
        kinds = scenario["sites"][mine - 1][0], scenario["sites"][theirs - 1][0]
        if kinds == ("cone", "cone") and route["route"] != "direct":
            raise SystemExit(f"{scenario['name']}: cone pair P{report['player_id']}->P{route['peer']} "
                             f"on {route['route']}")


def record_only(scenario, reports):
    pass


SCENARIOS = [
    {"name": "cone-cone", "sites": [("cone", 1), ("cone", 1)], "lobby": "CC", "frames": 180,
     "check": expect_all("direct")},
    {"name": "symmetric-symmetric", "sites": [("symmetric", 1), ("symmetric", 1)], "lobby": "SS",
     "frames": 180, "check": expect_all("relay")},
    {"name": "cone-symmetric", "sites": [("cone", 1), ("symmetric", 1)], "lobby": "CS",
     "frames": 180, "check": record_only},
    {"name": "same-lan", "sites": [("cone", 2)], "lobby": "LAN", "frames": 180,
     "check": expect_all("direct", via_lan)},
    {"name": "three-players-mixed", "sites": [("cone", 1), ("cone", 1), ("symmetric", 1)],
     "lobby": "MIX", "frames": 180, "check": cone_pair_direct},
    {"name": "direct-cut-mid-match", "sites": [("cone", 1), ("cone", 1)], "lobby": "CUT",
     "frames": 1200, "event": "cut-direct", "event_after_s": 8,
     "check": expect_all("relay", direct_then_fallback)},
    {"name": "server-lost-while-direct", "sites": [("cone", 1), ("cone", 1)], "lobby": "KILL",
     "frames": 1200, "event": "kill-server", "event_after_s": 8,
     "check": expect_all("direct")},
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("headless", type=Path)
    parser.add_argument("services", type=Path, help="services/matchmaking")
    parser.add_argument("--only", help="run one scenario by name")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    summary = []
    for scenario in SCENARIOS:
        if args.only and scenario["name"] != args.only:
            continue
        print(f"scenario {scenario['name']}: sites {scenario['sites']}", flush=True)
        reports = play(scenario, args.headless.resolve(), args.services.resolve())
        common_checks(scenario, reports)
        scenario["check"](scenario, reports)
        routes = sorted((r["player_id"], route["peer"], route["route"], route["via"],
                         route["switches"], route["fallbacks"])
                        for r, route in all_routes(reports))
        line = (f"PASS {scenario['name']}: checksum {reports[0]['checksum']:08x}; "
                + "; ".join(f"P{a}->P{b} {kind} via {via} (switches {s}, fallbacks {f})"
                            for a, b, kind, via, s, f in routes))
        print(line, flush=True)
        summary.append({"scenario": scenario["name"], "checksum": reports[0]["checksum"],
                        "routes": [dict(zip(("player", "peer", "route", "via", "switches", "fallbacks"), r))
                                   for r in routes]})
    if args.report:
        args.report.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
