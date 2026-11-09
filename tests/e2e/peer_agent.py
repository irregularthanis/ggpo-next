#!/usr/bin/env python3
"""Supervise one native smoke peer through an authenticated local control API."""

import argparse
import json
import socketserver
import subprocess
import threading
from pathlib import Path

from control import MessageVerifier


class PeerSupervisor:
    """Execute only the fixed peer lifecycle inside one configured workspace."""

    def __init__(self, workspace):
        self.workspace = Path(workspace).resolve()
        self.process = None
        self.command = None
        self._log = None

    def _close_log(self):
        if self._log is not None:
            self._log.close()
            self._log = None

    def safe_path(self, value):
        candidate = (self.workspace / value).resolve()
        try:
            candidate.relative_to(self.workspace)
        except ValueError as error:
            raise ValueError("path escapes agent workspace") from error
        return candidate

    def prepare(self, arguments):
        executable = self.safe_path(arguments["executable"])
        result = self.safe_path(arguments["result"])
        ready = self.safe_path(arguments["ready"])
        finalize = self.safe_path(arguments["finalize"])
        if not executable.is_file():
            raise ValueError("peer executable does not exist")
        result.parent.mkdir(parents=True, exist_ok=True)
        for artifact in (result, ready, finalize):
            if artifact.exists():
                artifact.unlink()
        self.command = {"executable": executable, "result": result,
                        "ready": ready, "finalize": finalize}
        return {"prepared": True}

    def launch_peer(self, arguments):
        if self.command is None or self.process is not None:
            raise ValueError("agent is not prepared or peer already launched")
        required = ("player", "local_port", "remote_port", "frames", "remote_ip",
                    "scenario", "run_id", "session_id", "platform")
        if any(name not in arguments for name in required):
            raise ValueError("launch-peer arguments are incomplete")
        command = [str(self.command["executable"]), str(arguments["player"]),
                   str(arguments["local_port"]), str(arguments["remote_port"]),
                   str(arguments["frames"]), str(self.command["result"]),
                   arguments["remote_ip"]]
        environment = dict(__import__("os").environ)
        environment.update({
            "GGPO_SMOKE_SCENARIO": arguments["scenario"],
            "GGPO_SMOKE_RUN_ID": arguments["run_id"],
            "GGPO_SMOKE_SESSION_ID": arguments["session_id"],
            "GGPO_SMOKE_PLATFORM": arguments["platform"],
            "GGPO_SMOKE_READY_FILE": str(self.command["ready"]),
            "GGPO_SMOKE_FINALIZE_FILE": str(self.command["finalize"]),
        })
        log = self.safe_path(arguments.get("log", "peer.log"))
        self._log = log.open("w", encoding="utf-8")
        self.process = subprocess.Popen(command, cwd=self.workspace, env=environment,
                                        stdout=self._log, stderr=subprocess.STDOUT)
        return {"pid": self.process.pid}

    def status(self, _arguments):
        return {
            "prepared": self.command is not None,
            "running": self.process is not None and self.process.poll() is None,
            "returncode": None if self.process is None else self.process.poll(),
            "ready": bool(self.command and self.command["ready"].exists()),
        }

    def collect(self, _arguments):
        if self.process is None:
            raise ValueError("peer has not launched")
        if _arguments.get("finalize"):
            self.command["finalize"].write_text("finalize\n", encoding="utf-8")
            try:
                self.process.wait(timeout=int(_arguments.get("timeout", 20)))
            except subprocess.TimeoutExpired as error:
                raise ValueError("peer did not exit before collection deadline") from error
        if self.process is None or self.process.poll() is None:
            raise ValueError("peer has not exited")
        if self.process.returncode != 0:
            raise ValueError(f"peer exited with {self.process.returncode}")
        if not self.command["result"].is_file():
            raise ValueError("peer result is missing")
        self._close_log()
        return json.loads(self.command["result"].read_text(encoding="utf-8"))

    def cancel(self, _arguments):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        self._close_log()
        return {"cancelled": True}

    def shutdown(self, arguments):
        self.cancel(arguments)
        return {"shutdown": True}

    def dispatch(self, operation, arguments):
        method = getattr(self, operation.replace("-", "_"), None)
        if method is None:
            raise ValueError("operation not implemented")
        return method(arguments)


class ControlHandler(socketserver.StreamRequestHandler):
    """Handle newline-delimited JSON requests; one connection owns one nonce stream."""

    def handle(self):
        for line in self.rfile:
            try:
                with self.server.verifier_lock:
                    operation, arguments = self.server.verifier.verify(json.loads(line))
                response = {"ok": True,
                            "result": self.server.supervisor.dispatch(operation, arguments)}
                if operation == "shutdown":
                    threading.Thread(target=self.server.shutdown, daemon=True).start()
            except (KeyError, OSError, RuntimeError, ValueError,
                    json.JSONDecodeError) as error:
                response = {"ok": False, "error": str(error)}
            self.wfile.write((json.dumps(response, sort_keys=True) + "\n").encode("utf-8"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--session-id", required=True)
    parser.add_argument("--key-hex", required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=24871)
    args = parser.parse_args()
    server = socketserver.ThreadingTCPServer((args.bind, args.port), ControlHandler)
    server.daemon_threads = True
    server.key = bytes.fromhex(args.key_hex)
    server.session_id = args.session_id
    server.verifier = MessageVerifier(server.key, server.session_id)
    server.verifier_lock = threading.Lock()
    server.supervisor = PeerSupervisor(args.workspace)
    try:
        server.serve_forever()
    finally:
        server.supervisor.cancel({})
        server.server_close()


if __name__ == "__main__":
    main()
