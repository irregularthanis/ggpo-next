#!/usr/bin/env python3
"""Run and validate one public multi-platform Vector War relay match."""

import argparse
import json
import os
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

try:
    from .rendezvous import DeploymentRendezvous, GitHubTransport
except ImportError:  # Direct script execution from tests/e2e.
    from rendezvous import DeploymentRendezvous, GitHubTransport


PLATFORMS = ("linux", "windows", "android")
PATH_LINE = re.compile(r"vectorwar-path: player=(\d+) peer=(\d+) route=(\w+)")


def split_server(value):
    """Split the deployment's DNS name and lobby port."""
    host, separator, port = value.rpartition(":")
    if not separator or not host or not port.isdigit() or not 1 <= int(port) <= 65535:
        raise ValueError("server must be HOST:PORT")
    return host, int(port)


def version_probe(server, timeout=3):
    """Resolve the DDNS name afresh and require a lobby protocol response."""
    host, port = split_server(server)
    probe = b"VWMM" + bytes([99, 1, 0, 0]) + struct.pack("<Q", 7) + bytes(48)
    addresses = socket.getaddrinfo(host, port, socket.AF_INET, socket.SOCK_DGRAM)
    if not addresses:
        raise OSError(f"{host} did not resolve")
    last_error = None
    for family, kind, protocol, _, address in addresses:
        with socket.socket(family, kind, protocol) as client:
            client.settimeout(timeout)
            try:
                client.sendto(probe, address)
                reply = client.recv(64)
            except OSError as error:
                last_error = error
                continue
            if reply[:4] == b"VWMM" and reply[5] == 7 and reply[16] == 1:
                return address[0]
            last_error = OSError("server returned an unexpected protocol reply")
    raise OSError(f"{server} did not answer: {last_error}")


def rendezvous(session_id):
    """Address one run-scoped transient GitHub Deployment record."""
    required = ("GITHUB_REPOSITORY", "GITHUB_SHA", "GITHUB_RUN_ID",
                "GITHUB_RUN_ATTEMPT", "GITHUB_TOKEN")
    missing = [name for name in required if not os.environ.get(name)]
    if missing:
        raise RuntimeError(f"missing environment: {', '.join(missing)}")
    return DeploymentRendezvous(
        GitHubTransport(os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_TOKEN"],
                        os.environ.get("GITHUB_API_URL", "https://api.github.com")),
        os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"],
        os.environ["GITHUB_RUN_ID"], os.environ["GITHUB_RUN_ATTEMPT"], session_id)


def probe_command(args):
    """Report a neutral skip only before a public match has started."""
    attempts = 12 if args.mode == "required" else 3
    error = None
    address = None
    for attempt in range(attempts):
        try:
            address = version_probe(args.server)
            break
        except (OSError, ValueError) as caught:
            error = caught
            if attempt + 1 < attempts:
                time.sleep(5)
    if address is None:
        if os.environ.get("GITHUB_OUTPUT"):
            with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
                output.write("run=false\n")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            Path(os.environ["GITHUB_STEP_SUMMARY"]).write_text(
                f"### Public multi-platform game not started\n\n{error}\n", encoding="utf-8")
        print(f"public server unavailable: {error}")
        return 1 if args.mode == "required" else 0
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            output.write(f"run=true\naddress={address}\n")
    print(f"public server ready: {args.server} resolved to {address}")
    return 0


def peer_command(executable, server, player_count, lobby, result, launcher=None):
    """The relay-only peer command; a launcher (Android: run_on_device.py)
    runs it on the emulator, mapping the result path and pulling it back."""
    command = [str(Path(executable).resolve()), "--matchmaking", server,
               str(player_count), lobby, "180", str(Path(result).resolve()),
               "--relay-only"]
    if launcher:
        command = [sys.executable, launcher] + command
    return command


def role_command(args):
    """Synchronize all platform runners, then run this platform's relay peer."""
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    own = rendezvous(f"{args.lobby}-{args.platform}")
    deployment_id = None
    try:
        deployment_id, _ = own.publish(args.platform, 0)
        for platform in PLATFORMS:
            rendezvous(f"{args.lobby}-{platform}").discover(deadline_seconds=600)

        result = output / "result.json"
        player_count = len(PLATFORMS)
        command = peer_command(args.executable, args.server, player_count, args.lobby,
                               result, os.environ.get("GGPO_TEST_LAUNCHER"))
        environment = dict(os.environ, VW_HEADLESS_FRAME_MS="16")
        completed = subprocess.run(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True,
                                   timeout=180, env=environment, check=False)
        (output / "run.log").write_text(completed.stdout, encoding="utf-8")
        routes = [match.group(3) for match in PATH_LINE.finditer(completed.stdout)]
        metadata = {"platform": args.platform, "exit_code": completed.returncode,
                    "routes": routes, "server": args.server, "lobby": args.lobby}
        (output / "metadata.json").write_text(
            json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        print(completed.stdout, end="")
        if completed.returncode:
            raise RuntimeError(f"{args.platform} peer exited {completed.returncode}")
        if routes != ["relay"] * (player_count - 1):
            raise RuntimeError(
                f"{args.platform} did not use {player_count - 1} relay paths: {routes}")
        return 0
    finally:
        if deployment_id is not None:
            own.retire(deployment_id)
            own.delete(deployment_id)


def validate_reports(root):
    """Require one converged result from every configured platform."""
    records = []
    for metadata_path in Path(root).rglob("metadata.json"):
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        result_path = metadata_path.with_name("result.json")
        if not result_path.exists():
            raise ValueError(f"{metadata['platform']}: missing result.json")
        records.append((metadata, json.loads(result_path.read_text(encoding="utf-8"))))
    platforms = {metadata["platform"] for metadata, _ in records}
    player_count = len(PLATFORMS)
    if len(records) != player_count or platforms != set(PLATFORMS):
        raise ValueError(f"expected {PLATFORMS}, found {sorted(platforms)}")
    players = {result.get("player") for _, result in records}
    if players != set(range(1, player_count + 1)):
        raise ValueError(f"unexpected player assignment: {players}")
    if {result.get("frame") for _, result in records} != {180}:
        raise ValueError("not every peer reached frame 180")
    checksums = {result.get("checksum") for _, result in records}
    if len(checksums) != 1:
        raise ValueError(f"peers diverged: {checksums}")
    for metadata, result in records:
        if (metadata.get("exit_code") != 0 or
                metadata.get("routes") != ["relay"] * (player_count - 1)):
            raise ValueError(f"{metadata['platform']}: invalid process or route evidence")
        if result.get("running_events") != 1:
            raise ValueError(f"{metadata['platform']}: expected one running event")
    if sum(result.get("replay_frames", 0) for _, result in records) < 1:
        raise ValueError("the match did not exercise rollback replay")
    return {"result": "pass", "platforms": list(PLATFORMS), "players": player_count,
            "frame": 180, "checksum": checksums.pop(), "transport": "forced-relay",
            "peers": [{"platform": metadata["platform"], **result}
                      for metadata, result in records]}


def validate_command(args):
    manifest = validate_reports(args.evidence)
    Path(args.output).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"PASS multi-platform game: checksum={manifest['checksum']:08x}, all paths relay")
    return 0


def main():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    probe = commands.add_parser("probe")
    probe.add_argument("--server", required=True)
    probe.add_argument("--mode", choices=("optional", "required"), required=True)
    probe.set_defaults(function=probe_command)
    role = commands.add_parser("role")
    role.add_argument("--server", required=True)
    role.add_argument("--lobby", required=True)
    role.add_argument("--platform", choices=PLATFORMS, required=True)
    role.add_argument("--executable", required=True)
    role.add_argument("--output", required=True)
    role.set_defaults(function=role_command)
    validate = commands.add_parser("validate")
    validate.add_argument("--evidence", required=True)
    validate.add_argument("--output", required=True)
    validate.set_defaults(function=validate_command)
    args = parser.parse_args()
    return args.function(args)


if __name__ == "__main__":
    sys.exit(main())
