#!/usr/bin/env python3
"""Regression cases for the portable peer-result and checksum contract."""

import copy
import unittest

from result_schema import canonical_bytes, canonical_checksum, expected_state, validate_pair


def valid_peer(player, platform):
    checksum = expected_state(180)["canonical_checksum"]
    return {
        "schema": 2,
        "scenario": "linux-windows",
        "run_id": "run-42",
        "session_id": "session-7",
        "player": player,
        "platform": platform,
        "frames": 180,
        "canonical_checksum": checksum,
        "native_checksum": checksum,
        "saves": 2,
        "loads": 0,
        "replay_frames": 0,
        "running_events": 1,
        "unexpected_disconnects": 0,
        "session_closed": True,
        "exit_code": 0,
    }


class ResultSchemaTests(unittest.TestCase):
    def setUp(self):
        self.peer_a = valid_peer(1, "linux")
        self.peer_b = valid_peer(2, "windows")

    def validate(self, peer_a=None, peer_b=None):
        return validate_pair(peer_a or self.peer_a, peer_b or self.peer_b, 180,
                             scenario="linux-windows", run_id="run-42",
                             session_id="session-7",
                             platforms=("linux", "windows"))

    def test_canonical_vectors_are_fixed(self):
        self.assertEqual(canonical_bytes(1, 245).hex(),
                         "01000000f500000000000000")
        self.assertEqual(canonical_checksum(0, 0), 0xE23C62B5)
        self.assertEqual(canonical_checksum(1, 245), 0x81D50411)
        self.assertEqual(expected_state(90)["canonical_checksum"], 0x8FE1122C)
        self.assertEqual(expected_state(180)["canonical_checksum"], 0x7DB6C67B)

    def test_valid_schema2_pair_passes(self):
        self.assertEqual(self.validate()["result"], "pass")

    def test_legacy_schema_remains_accepted(self):
        peers = [
            {"player": 1, "frame": 180, "checksum": 7, "running_events": 1},
            {"player": 2, "frame": 180, "checksum": 7, "running_events": 1},
        ]
        self.assertEqual(validate_pair(*peers, 180)["schema"], 1)

    def assert_rejected(self, field, value, message):
        peer = copy.deepcopy(self.peer_b)
        if value is None:
            del peer[field]
        else:
            peer[field] = value
        with self.assertRaisesRegex(ValueError, message):
            self.validate(peer_b=peer)

    def test_missing_field_is_rejected(self):
        self.assert_rejected("session_closed", None, "missing fields")

    def test_wrong_platform_role_is_rejected(self):
        self.assert_rejected("platform", "linux", "platform-role mismatch")

    def test_wrong_run_identity_is_rejected(self):
        self.assert_rejected("run_id", "stale-run", "run_id mismatch")

    def test_noncanonical_checksum_is_rejected(self):
        self.assert_rejected("canonical_checksum", 1,
                             "canonical checksum mismatch")

    def test_disconnect_is_rejected(self):
        self.assert_rejected("unexpected_disconnects", 1,
                             "unexpected disconnect")

    def test_unclean_close_is_rejected(self):
        self.assert_rejected("session_closed", False,
                             "did not close cleanly")


if __name__ == "__main__":
    unittest.main()
