#!/usr/bin/env python3
"""Coordinate role 2 locally against a published role-1 native peer agent."""

import argparse
import json
import os
import time
from pathlib import Path

from control import derive_key
from control_client import ControlClient
from peer_agent import PeerSupervisor
from rendezvous import DeploymentRendezvous, GitHubTransport
from result_schema import expected_state, validate_pair


def required_environment():
    names = ("GITHUB_REPOSITORY", "GITHUB_SHA", "GITHUB_RUN_ID",
             "GITHUB_RUN_ATTEMPT", "GITHUB_TOKEN", "GGPO_E2E_SESSION_SECRET")
    missing = [name for name in names if not os.environ.get(name)]
    if missing:
        raise SystemExit(f"missing environment: {', '.join(missing)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--player1-executable", required=True)
    parser.add_argument("--local-address", required=True)
    # Large matrices queue role jobs independently across OS runner pools.
    parser.add_argument("--rendezvous-timeout", type=int, default=1200)
    parser.add_argument("--session-id", required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--player1-platform", required=True)
    parser.add_argument("--player2-platform", required=True)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--fixture", choices=("smoke", "vectorwar"),
                        default="smoke")
    parser.add_argument("--player1-port", type=int, default=24872)
    parser.add_argument("--player2-port", type=int, default=24873)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    required_environment()

    key = derive_key(os.environ["GGPO_E2E_SESSION_SECRET"],
                     os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"],
                     os.environ["GITHUB_RUN_ID"], os.environ["GITHUB_RUN_ATTEMPT"])
    rendezvous = DeploymentRendezvous(
        GitHubTransport(os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_TOKEN"],
                        os.environ.get("GITHUB_API_URL", "https://api.github.com")),
        os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"],
        os.environ["GITHUB_RUN_ID"], os.environ["GITHUB_RUN_ATTEMPT"],
        args.session_id,
    )
    _, endpoint = rendezvous.discover(deadline_seconds=args.rendezvous_timeout)
    workspace = args.workspace.resolve()
    local = PeerSupervisor(workspace)
    local.prepare({"executable": args.executable, "result": "evidence/player2.json",
                   "ready": "evidence/player2.ready",
                   "finalize": "evidence/player2.finalize"})
    run_id = f"{os.environ['GITHUB_RUN_ID']}-{os.environ['GITHUB_RUN_ATTEMPT']}"
    common = {"scenario": args.scenario, "run_id": run_id,
              "session_id": args.session_id, "frames": args.frames}
    expected_checksum = expected_state(args.frames, args.fixture)["canonical_checksum"]
    remote_prepared = False
    shutdown_sent = False
    try:
        with ControlClient(endpoint["address"], endpoint["control_port"], key,
                           args.session_id) as remote:
            try:
                remote.request("prepare", {
                    "executable": args.player1_executable,
                    "result": "evidence/player1.json",
                    "ready": "evidence/player1.ready",
                    "finalize": "evidence/player1.finalize",
                })
                remote_prepared = True
                remote.request("launch-peer", {
                    **common, "player": 1, "platform": args.player1_platform,
                    "local_port": args.player1_port, "remote_port": args.player2_port,
                    "remote_ip": args.local_address, "log": "evidence/player1.log",
                })
                local.launch_peer({
                    **common, "player": 2, "platform": args.player2_platform,
                    "local_port": args.player2_port, "remote_port": args.player1_port,
                    "remote_ip": endpoint["address"], "log": "evidence/player2.log",
                })
                deadline = time.monotonic() + 60
                while time.monotonic() < deadline:
                    remote_status = remote.request("status")
                    local_status = local.status({})
                    statuses = (remote_status, local_status)
                    if all(
                        status["ready"] and status["frame"] == args.frames and
                        status["canonical_checksum"] == expected_checksum
                        for status in statuses
                    ):
                        break
                    if remote_status["returncode"] not in (None, 0) or \
                            local_status["returncode"] not in (None, 0):
                        raise RuntimeError("peer exited before the completion barrier")
                    time.sleep(0.1)
                else:
                    raise TimeoutError("both peers did not reach the completion barrier")

                # Release both peers before collecting either result so one
                # side cannot keep correcting state after the other exits.
                local.finalize({})
                remote.request("finalize")
                player1 = remote.request("collect", {"timeout": 20})
                local.process.wait(timeout=20)
                player2 = local.collect({})
                report = validate_pair(
                    player1, player2, args.frames, scenario=args.scenario,
                    run_id=run_id, session_id=args.session_id,
                    platforms=(args.player1_platform, args.player2_platform),
                    fixture=args.fixture,
                )
                args.out.parent.mkdir(parents=True, exist_ok=True)
                args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                    encoding="utf-8")
                remote.request("shutdown")
                shutdown_sent = True
                print(json.dumps(report, indent=2, sort_keys=True))
            finally:
                if remote_prepared and not shutdown_sent:
                    try:
                        remote.request("cancel")
                        remote.request("shutdown")
                    except (OSError, RuntimeError):
                        pass
    finally:
        local.cancel({})


if __name__ == "__main__":
    main()
