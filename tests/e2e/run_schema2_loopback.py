#!/usr/bin/env python3
"""Exercise schema 2 and the completion barrier with two local native peers."""

import argparse
import json
import socket
import tempfile
import time
from pathlib import Path

from peer_agent import PeerSupervisor
from result_schema import validate_pair


def reserve_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    args = parser.parse_args()
    port1, port2 = reserve_port(), reserve_port()
    while port2 == port1:
        port2 = reserve_port()

    with tempfile.TemporaryDirectory(prefix="ggpo-schema2-") as directory:
        root = Path(directory)
        executable = root / args.executable.name
        executable.write_bytes(args.executable.read_bytes())
        executable.chmod(0o755)
        peers = [PeerSupervisor(root), PeerSupervisor(root)]
        for player, peer in enumerate(peers, start=1):
            peer.prepare({"executable": executable.name,
                          "result": f"player{player}.json",
                          "ready": f"player{player}.ready",
                          "finalize": f"player{player}.finalize"})
            peer.launch_peer({
                "player": player,
                "local_port": port1 if player == 1 else port2,
                "remote_port": port2 if player == 1 else port1,
                "frames": args.frames,
                "remote_ip": "127.0.0.1",
                "scenario": "linux-linux",
                "run_id": "local-1",
                "session_id": "schema2-loopback",
                "platform": "linux",
                "log": f"player{player}.log",
            })
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                statuses = [peer.status({}) for peer in peers]
                if all(status["ready"] for status in statuses):
                    break
                if any(status["returncode"] not in (None, 0) for status in statuses):
                    raise RuntimeError(f"peer exited before barrier: {statuses}")
                time.sleep(0.05)
            else:
                raise TimeoutError("schema 2 peers did not reach barrier")
            results = [peer.collect({"finalize": True, "timeout": 20})
                       for peer in peers]
            report = validate_pair(
                results[0], results[1], args.frames,
                scenario="linux-linux", run_id="local-1",
                session_id="schema2-loopback", platforms=("linux", "linux"),
            )
            print(json.dumps(report, indent=2, sort_keys=True))
        finally:
            for peer in peers:
                peer.cancel({})


if __name__ == "__main__":
    main()
