#!/usr/bin/env python3
"""Small persistent client for the authenticated peer-agent protocol."""

import json
import socket

from control import sign_message


class ControlClient:
    def __init__(self, host, port, key, session_id, timeout=20):
        self.socket = socket.create_connection((host, int(port)), timeout=timeout)
        self.stream = self.socket.makefile("rwb")
        self.key = key
        self.session_id = session_id
        self.nonce = 0

    def request(self, operation, arguments=None):
        self.nonce += 1
        message = sign_message(self.key, self.session_id, self.nonce,
                               operation, arguments)
        self.stream.write((json.dumps(message, sort_keys=True) + "\n").encode("utf-8"))
        self.stream.flush()
        line = self.stream.readline()
        if not line:
            raise RuntimeError("peer agent closed the control connection")
        response = json.loads(line)
        if not response.get("ok"):
            raise RuntimeError(response.get("error", "peer agent request failed"))
        return response["result"]

    def close(self):
        self.stream.close()
        self.socket.close()

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()
