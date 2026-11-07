#!/usr/bin/env python3
"""Negative and completeness tests for cross-host control orchestration."""

import copy
import tempfile
import unittest
from pathlib import Path

from control import MessageVerifier, derive_key, sign_message
from peer_agent import PeerSupervisor
from run_matrix import generate_matrix, validate_matrix


class ControlTests(unittest.TestCase):
    def setUp(self):
        self.key = derive_key("secret", "owner/repo", "abc", "42", "1")
        self.message = sign_message(self.key, "session", 1, "status")

    def test_valid_message_passes(self):
        self.assertEqual(MessageVerifier(self.key, "session").verify(self.message),
                         ("status", {}))

    def test_bad_mac_fails(self):
        message = copy.deepcopy(self.message)
        message["mac"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "invalid control MAC"):
            MessageVerifier(self.key, "session").verify(message)

    def test_wrong_session_fails(self):
        with self.assertRaisesRegex(ValueError, "wrong control session"):
            MessageVerifier(self.key, "other").verify(self.message)

    def test_replay_fails(self):
        verifier = MessageVerifier(self.key, "session")
        verifier.verify(self.message)
        with self.assertRaisesRegex(ValueError, "replayed"):
            verifier.verify(self.message)

    def test_unknown_operation_cannot_be_signed(self):
        with self.assertRaisesRegex(ValueError, "not allowed"):
            sign_message(self.key, "session", 1, "shell", {"command": "whoami"})


class MatrixTests(unittest.TestCase):
    def test_complete_ordered_matrix(self):
        matrix = validate_matrix(generate_matrix())
        self.assertEqual([item["scenario"] for item in matrix], [
            "linux-linux", "linux-windows", "windows-linux", "windows-windows",
        ])

    def test_missing_and_duplicate_scenarios_fail(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            validate_matrix(generate_matrix()[:-1])
        duplicate = generate_matrix() + [generate_matrix()[0]]
        with self.assertRaisesRegex(ValueError, "duplicate"):
            validate_matrix(duplicate)


class SupervisorTests(unittest.TestCase):
    def test_workspace_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            with self.assertRaisesRegex(ValueError, "escapes"):
                supervisor.safe_path("../outside")

    def test_missing_executable_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            with self.assertRaisesRegex(ValueError, "does not exist"):
                supervisor.prepare({
                    "executable": "missing", "result": "out/result.json",
                    "ready": "out/ready", "finalize": "out/finalize",
                })


if __name__ == "__main__":
    unittest.main()
