#!/usr/bin/env python3
"""Contract tests for transient GitHub Deployment rendezvous lifecycle."""

import unittest

from rendezvous import DeploymentRendezvous


class FakeTransport:
    def __init__(self):
        self.calls = []
        self.deployments = []

    def request(self, method, path, payload=None):
        self.calls.append((method, path, payload))
        if method == "POST" and path == "/deployments":
            deployment = dict(payload)
            deployment["id"] = len(self.deployments) + 1
            self.deployments.insert(0, deployment)
            return deployment
        if method == "GET" and path.startswith("/deployments?"):
            return list(self.deployments)
        if method == "POST" and "/statuses" in path:
            return {"state": payload["state"]}
        raise AssertionError((method, path, payload))


class RendezvousTests(unittest.TestCase):
    def setUp(self):
        self.transport = FakeTransport()
        self.rendezvous = DeploymentRendezvous(
            self.transport, "owner/repo", "abc123", "42", "1", "linux-windows"
        )

    def test_publish_discover_retire_lifecycle(self):
        deployment_id, record = self.rendezvous.publish("100.64.0.1", 24871)
        self.assertEqual(record["control_port"], 24871)
        found_id, found = self.rendezvous.discover(deadline_seconds=0.1,
                                                   poll_seconds=0)
        self.assertEqual((found_id, found), (deployment_id, record))
        self.assertEqual(self.rendezvous.retire(deployment_id)["state"], "inactive")

    def test_wrong_identity_is_ignored_and_times_out(self):
        valid = {"repository": "owner/repo", "sha": "abc123",
                 "run_id": "42", "attempt": "1",
                 "session_id": "linux-windows"}
        for field, wrong in (("repository", "other/repo"), ("sha", "wrong"),
                             ("run_id", "43"), ("attempt", "2"),
                             ("session_id", "windows-linux")):
            payload = dict(valid)
            payload[field] = wrong
            self.transport.deployments = [{
                "id": 1, "environment": self.rendezvous.environment,
                "payload": payload,
            }]
            with self.subTest(field=field), self.assertRaisesRegex(
                    TimeoutError, "before deadline"):
                self.rendezvous.discover(deadline_seconds=0, poll_seconds=0)

    def test_duplicate_exact_records_fail_closed(self):
        self.rendezvous.publish("100.64.0.1", 24871)
        self.rendezvous.publish("100.64.0.2", 24871)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.rendezvous.discover(deadline_seconds=0.1, poll_seconds=0)


if __name__ == "__main__":
    unittest.main()
