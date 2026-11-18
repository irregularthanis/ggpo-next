#!/usr/bin/env python3
"""The end-of-run cleanup removes exactly this run's rendezvous records."""

import unittest

from rendezvous_cleanup import cleanup


class FakeTransport:
    def __init__(self, deployments, fail_delete=()):
        self.deployments = deployments
        self.fail_delete = set(fail_delete)
        self.calls = []

    def request(self, method, path, payload=None):
        self.calls.append((method, path))
        if method == "GET":
            page = int(path.rsplit("page=", 1)[1])
            return self.deployments[(page - 1) * 100:page * 100]
        if method == "DELETE":
            identifier = int(path.rsplit("/", 1)[1])
            if identifier in self.fail_delete:
                raise RuntimeError("GitHub API DELETE failed: 422")
            return None
        return {"state": payload["state"]}


def deployment(identifier, environment, run_id, attempt):
    return {"id": identifier, "environment": environment,
            "payload": {"run_id": str(run_id), "attempt": str(attempt)}}


class CleanupTests(unittest.TestCase):
    def test_only_this_run_and_attempt_is_removed(self):
        transport = FakeTransport([
            deployment(1, "ggpo-e2e-linux-windows", 42, 1),
            deployment(2, "ggpo-e2e-vectorwar-macos-linux", 42, 1),
            deployment(3, "ggpo-e2e-linux-windows", 42, 2),   # a later attempt
            deployment(4, "ggpo-e2e-linux-windows", 420, 1),  # another run, same environment
            deployment(5, "matchmaking", 42, 1),              # not a rendezvous
        ])
        removed, failures = cleanup(transport, "abc", "42", "1")
        self.assertEqual((removed, failures), (2, []))
        deleted = [path for method, path in transport.calls if method == "DELETE"]
        self.assertEqual(deleted, ["/deployments/1", "/deployments/2"])

    def test_each_deployment_is_made_inactive_before_delete(self):
        transport = FakeTransport([deployment(7, "ggpo-e2e-a", 9, 1)])
        cleanup(transport, "abc", "9", "1")
        self.assertEqual([c for c in transport.calls if c[0] != "GET"], [
            ("POST", "/deployments/7/statuses"), ("DELETE", "/deployments/7")])

    def test_every_page_is_read(self):
        records = [deployment(i, "ggpo-e2e-x", 5, 1) for i in range(1, 251)]
        removed, _ = cleanup(FakeTransport(records), "abc", "5", "1")
        self.assertEqual(removed, 250)

    def test_a_failed_delete_is_reported_not_raised(self):
        transport = FakeTransport([deployment(1, "ggpo-e2e-a", 3, 1),
                                   deployment(2, "ggpo-e2e-b", 3, 1)], fail_delete={1})
        removed, failures = cleanup(transport, "abc", "3", "1")
        self.assertEqual(removed, 1)
        self.assertEqual(len(failures), 1)
        self.assertIn("1:", failures[0])


if __name__ == "__main__":
    unittest.main()
