#!/usr/bin/env python3
"""Authenticated control-message contract for native E2E peer agents."""

import hashlib
import hmac
import json


ALLOWED_OPERATIONS = {
    "prepare", "launch-peer", "status", "collect", "cancel", "shutdown",
}


def derive_key(secret, repository, sha, run_id, attempt):
    """Bind the workflow secret to one repository revision and run attempt."""
    context = f"{repository}\n{sha}\n{run_id}\n{attempt}".encode("utf-8")
    return hmac.new(secret.encode("utf-8"), context, hashlib.sha256).digest()


def _payload(message):
    unsigned = {key: value for key, value in message.items() if key != "mac"}
    return json.dumps(unsigned, sort_keys=True, separators=(",", ":")).encode("utf-8")


def sign_message(key, session_id, nonce, operation, arguments=None):
    """Create one canonical HMAC-authenticated control request."""
    if operation not in ALLOWED_OPERATIONS:
        raise ValueError(f"operation not allowed: {operation}")
    message = {
        "session_id": session_id,
        "nonce": nonce,
        "operation": operation,
        "arguments": arguments or {},
    }
    message["mac"] = hmac.new(key, _payload(message), hashlib.sha256).hexdigest()
    return message


class MessageVerifier:
    """Reject invalid identity, HMAC, operations, and replayed/out-of-order requests."""

    def __init__(self, key, session_id):
        self.key = key
        self.session_id = session_id
        self.last_nonce = -1

    def verify(self, message):
        required = {"session_id", "nonce", "operation", "arguments", "mac"}
        if not isinstance(message, dict) or set(message) != required:
            raise ValueError("control message has unexpected fields")
        if message["session_id"] != self.session_id:
            raise ValueError("wrong control session")
        if message["operation"] not in ALLOWED_OPERATIONS:
            raise ValueError("operation not allowed")
        nonce = message["nonce"]
        if not isinstance(nonce, int) or nonce <= self.last_nonce:
            raise ValueError("replayed or out-of-order nonce")
        expected = hmac.new(self.key, _payload(message), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(message["mac"], expected):
            raise ValueError("invalid control MAC")
        if not isinstance(message["arguments"], dict):
            raise ValueError("control arguments must be an object")
        self.last_nonce = nonce
        return message["operation"], message["arguments"]
