#!/usr/bin/env python3
"""Publish one role-1 agent endpoint, serve it, then retire it unconditionally."""

import argparse
import os
import socketserver
import threading
from pathlib import Path

from control import MessageVerifier, derive_key
from peer_agent import ControlHandler, PeerSupervisor
from rendezvous import DeploymentRendezvous, GitHubTransport


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--address", required=True)
    parser.add_argument("--port", type=int, default=24871)
    parser.add_argument("--session-id", required=True)
    # The paired role can remain queued on a different runner pool.
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    required = ("GITHUB_REPOSITORY", "GITHUB_SHA", "GITHUB_RUN_ID",
                "GITHUB_RUN_ATTEMPT", "GITHUB_TOKEN", "GGPO_E2E_SESSION_SECRET")
    missing = [name for name in required if not os.environ.get(name)]
    if missing:
        raise SystemExit(f"missing environment: {', '.join(missing)}")

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
    server = socketserver.ThreadingTCPServer((args.address, args.port), ControlHandler)
    server.daemon_threads = True
    server.key = key
    server.session_id = args.session_id
    server.verifier = MessageVerifier(key, args.session_id)
    server.verifier_lock = threading.Lock()
    server.supervisor = PeerSupervisor(args.workspace)
    deployment_id = None
    timed_out = threading.Event()

    def stop_after_deadline():
        timed_out.set()
        server.shutdown()

    timer = threading.Timer(args.timeout, stop_after_deadline)
    try:
        deployment_id, _ = rendezvous.publish(args.address, args.port)
        print(f"published agent for {args.session_id} at {args.address}:{args.port}",
              flush=True)
        timer.start()
        server.serve_forever()
        if timed_out.is_set():
            raise TimeoutError(
                f"paired role did not connect within {args.timeout} seconds"
            )
    finally:
        timer.cancel()
        server.supervisor.cancel({})
        server.server_close()
        if deployment_id is not None:
            rendezvous.retire(deployment_id)
            rendezvous.delete(deployment_id)


if __name__ == "__main__":
    main()
