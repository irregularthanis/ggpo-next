#!/usr/bin/env python3
"""Negative and completeness tests for cross-host control orchestration."""

import copy
import io
import tempfile
import unittest
from pathlib import Path

from control import MessageVerifier, derive_key, sign_message
from peer_agent import PeerSupervisor
from run_schema2_loopback import stage_runtime_bundle
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

    def test_cancel_closes_log_after_peer_already_exited(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            log = io.StringIO()
            supervisor._log = log
            supervisor.cancel({})
            self.assertTrue(log.closed)

    def test_status_requires_a_complete_ready_state_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            ready = Path(directory) / "peer.ready"
            supervisor.command = {"ready": ready}
            ready.write_text("ready\n", encoding="utf-8")
            self.assertFalse(supervisor.status({})["ready"])
            ready.write_text(
                '{"frame":180,"canonical_checksum":1234}\n',
                encoding="utf-8",
            )
            status = supervisor.status({})
            self.assertTrue(status["ready"])
            self.assertEqual(status["frame"], 180)
            self.assertEqual(status["canonical_checksum"], 1234)

    def test_finalize_releases_a_prepared_peer(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            supervisor.command = {
                "finalize": Path(directory) / "peer.finalize",
            }
            supervisor.process = object()
            self.assertEqual(supervisor.finalize({}), {"finalized": True})
            self.assertEqual(
                supervisor.command["finalize"].read_text(encoding="utf-8"),
                "finalize\n",
            )

    def test_runtime_bundle_copies_adjacent_windows_library(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            destination = root / "staged"
            source.mkdir()
            executable = source / "peer.exe"
            library = source / "GGPO.dll"
            executable.write_bytes(b"peer")
            library.write_bytes(b"library")
            staged = stage_runtime_bundle(executable, destination)
            self.assertEqual(staged.read_bytes(), b"peer")
            self.assertEqual((destination / "GGPO.dll").read_bytes(), b"library")


if __name__ == "__main__":
    unittest.main()
