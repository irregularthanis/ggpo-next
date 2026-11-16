"""Focused contracts for the public three-platform game coordinator."""

import json
import tempfile
import unittest
from pathlib import Path

from .public_group import PLATFORMS, split_server, validate_reports


class PublicGroupTest(unittest.TestCase):
    def test_server_requires_host_and_port(self):
        self.assertEqual(split_server("vectorwars-demo.duckdns.org:7100"),
                         ("vectorwars-demo.duckdns.org", 7100))
        for invalid in ("", "host", "host:no", "host:0", "host:65536"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                split_server(invalid)

    def test_three_platform_manifest_requires_convergence_and_relay(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for player, platform in enumerate(PLATFORMS, start=1):
                peer = root / platform
                peer.mkdir()
                (peer / "metadata.json").write_text(json.dumps({
                    "platform": platform, "exit_code": 0,
                    "routes": ["relay", "relay"], "server": "host:7100",
                    "lobby": "TEST",
                }), encoding="utf-8")
                (peer / "result.json").write_text(json.dumps({
                    "player": player, "frame": 180, "checksum": 1234,
                    "running_events": 1, "replay_frames": 1 if player == 2 else 0,
                }), encoding="utf-8")
            manifest = validate_reports(root)
            self.assertEqual(manifest["result"], "pass")
            self.assertEqual(manifest["transport"], "forced-relay")

            result = root / "android" / "result.json"
            broken = json.loads(result.read_text(encoding="utf-8"))
            broken["checksum"] = 9999
            result.write_text(json.dumps(broken), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "diverged"):
                validate_reports(root)


if __name__ == "__main__":
    unittest.main()
