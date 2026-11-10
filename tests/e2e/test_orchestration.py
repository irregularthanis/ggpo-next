#!/usr/bin/env python3
"""Negative and completeness tests for cross-host control orchestration."""

import copy
import hashlib
import hmac
import io
import json
import tempfile
import unittest
from pathlib import Path

from control import MessageVerifier, _payload, derive_key, sign_message
from peer_agent import PeerSupervisor
from run_schema2_loopback import stage_runtime_bundle
from run_matrix import generate_matrix, validate_matrix
from validate_manifest import build_manifest


class ExitedProcess:
    def poll(self):
        return 17


class ControlTests(unittest.TestCase):
    def setUp(self):
        self.key = derive_key("secret", "owner/repo", "abc", "42", "1")
        self.message = sign_message(self.key, "session", 1, "status")

    def test_valid_message_passes(self):
        self.assertEqual(MessageVerifier(self.key, "session").verify(self.message),
                         ("status", {}))

    def test_finalize_is_an_authenticated_allow_list_operation(self):
        message = sign_message(self.key, "session", 1, "finalize")
        self.assertEqual(
            MessageVerifier(self.key, "session").verify(message),
            ("finalize", {}),
        )

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

    def test_unknown_raw_operation_and_non_object_arguments_fail(self):
        verifier = MessageVerifier(self.key, "session")
        unknown = copy.deepcopy(self.message)
        unknown["operation"] = "shell"
        with self.assertRaisesRegex(ValueError, "not allowed"):
            verifier.verify(unknown)
        malformed = sign_message(self.key, "session", 2, "status")
        malformed["arguments"] = []
        malformed["mac"] = hmac.new(
            self.key, _payload(malformed), hashlib.sha256
        ).hexdigest()
        with self.assertRaisesRegex(ValueError, "must be an object"):
            verifier.verify(malformed)

    def test_key_is_bound_to_repository_revision_and_run(self):
        contexts = (
            ("other/repo", "abc", "42", "1"),
            ("owner/repo", "def", "42", "1"),
            ("owner/repo", "abc", "43", "1"),
            ("owner/repo", "abc", "42", "2"),
        )
        for context in contexts:
            verifier = MessageVerifier(derive_key("secret", *context), "session")
            with self.assertRaisesRegex(ValueError, "invalid control MAC"):
                verifier.verify(copy.deepcopy(self.message))


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

    def test_manifest_rejects_missing_duplicate_and_failed_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def write_report(name, scenario, fixture="smoke", result="pass"):
                target = root / name / "report.json"
                target.parent.mkdir()
                target.write_text(
                    json.dumps({
                        "fixture": fixture, "scenario": scenario,
                        "result": result,
                    }),
                    encoding="utf-8",
                )

            write_report("ll", "linux-linux")
            with self.assertRaisesRegex(ValueError, "incomplete"):
                build_manifest(root)
            write_report("ww", "windows-windows")
            write_report("lw", "linux-windows")
            write_report("wl", "windows-linux")
            for scenario in ("linux-linux", "windows-windows",
                             "linux-windows", "windows-linux"):
                write_report(
                    f"vw-{scenario}", f"vectorwar-{scenario}",
                    fixture="vectorwar",
                )
            self.assertEqual(build_manifest(root)["result"], "pass")
            write_report("duplicate", "linux-linux")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                build_manifest(root)
            (root / "duplicate" / "report.json").unlink()
            write_report("failed", "linux-linux", result="fail")
            with self.assertRaisesRegex(ValueError, "non-passing"):
                build_manifest(root)


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

    def test_all_prepared_artifact_paths_must_stay_in_workspace(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "peer"
            executable.write_text("fixture", encoding="utf-8")
            for field in ("result", "ready", "finalize"):
                arguments = {
                    "executable": "peer", "result": "result.json",
                    "ready": "ready.json", "finalize": "finalize",
                }
                arguments[field] = "../escape"
                with self.assertRaisesRegex(ValueError, "escapes"):
                    PeerSupervisor(directory).prepare(arguments)

    def test_crashed_peer_is_visible_to_coordinator(self):
        with tempfile.TemporaryDirectory() as directory:
            supervisor = PeerSupervisor(directory)
            supervisor.process = ExitedProcess()
            status = supervisor.status({})
            self.assertFalse(status["running"])
            self.assertEqual(status["returncode"], 17)

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
