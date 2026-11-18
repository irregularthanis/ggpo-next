#!/usr/bin/env python3
"""Contract tests for transient GitHub Deployment rendezvous lifecycle."""

import unittest

import io
import urllib.error
from unittest import mock

from rendezvous import DeploymentRendezvous, GitHubTransport


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
        if method == "DELETE" and path.startswith("/deployments/"):
            return None
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
        self.assertIsNone(self.rendezvous.delete(deployment_id))
        self.assertEqual(
            self.transport.calls[-1],
            ("DELETE", "/deployments/1", None),
        )

    def test_each_scenario_has_an_isolated_deployment_environment(self):
        other = DeploymentRendezvous(
            self.transport, "owner/repo", "abc123", "42", "1",
            "windows-linux",
        )
        self.assertNotEqual(self.rendezvous.environment, other.environment)
        self.assertTrue(self.rendezvous.environment.endswith("linux-windows"))

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



class TransportRetryTests(unittest.TestCase):
    """GitHub 5xx responses: repeatable requests retry, creation does not."""

    def failing(self, codes):
        responses = iter(codes)

        def urlopen(request, timeout):
            code = next(responses)
            if code >= 400:
                raise urllib.error.HTTPError(request.full_url, code, "error", {},
                                             io.BytesIO(b"oops"))
            response = mock.MagicMock()
            response.__enter__.return_value.read.return_value = b""
            return response
        return urlopen

    def transport(self):
        self.sleeps = []
        return GitHubTransport("owner/repo", "token", sleep=self.sleeps.append)

    def test_delete_retries_a_server_error(self):
        transport = self.transport()
        with mock.patch("urllib.request.urlopen", self.failing([500, 502, 200])):
            self.assertIsNone(transport.request("DELETE", "/deployments/7"))
        self.assertEqual(self.sleeps, [1, 3])

    def test_retries_are_bounded(self):
        transport = self.transport()
        with mock.patch("urllib.request.urlopen", self.failing([500] * 4)):
            with self.assertRaisesRegex(RuntimeError, "500"):
                transport.request("GET", "/deployments?environment=x")
        self.assertEqual(self.sleeps, [1, 3, 9])

    def test_client_errors_are_not_retried(self):
        transport = self.transport()
        with mock.patch("urllib.request.urlopen", self.failing([404])):
            with self.assertRaisesRegex(RuntimeError, "404"):
                transport.request("DELETE", "/deployments/7")
        self.assertEqual(self.sleeps, [])

    def test_creating_a_deployment_is_never_retried(self):
        transport = self.transport()
        with mock.patch("urllib.request.urlopen", self.failing([500, 200])):
            with self.assertRaisesRegex(RuntimeError, "500"):
                transport.request("POST", "/deployments", {"ref": "abc"})
        self.assertEqual(self.sleeps, [])


if __name__ == "__main__":
    unittest.main()
