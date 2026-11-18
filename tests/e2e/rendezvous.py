#!/usr/bin/env python3
"""Run-scoped GitHub Deployment rendezvous for concurrent native runners."""

import json
import time
import urllib.error
import urllib.parse
import urllib.request


class GitHubTransport:
    """Minimal GitHub REST transport with bounded request timeouts."""

    # GitHub's API returns occasional 5xx. Lookups, retiring and deleting are
    # safe to repeat; creating a deployment is not (a retry could publish two
    # records), so that POST is never retried.
    RETRY_DELAYS = (1, 3, 9)

    def __init__(self, repository, token, api_url="https://api.github.com", timeout=15,
                 sleep=time.sleep):
        self.base = f"{api_url.rstrip('/')}/repos/{repository}"
        self.token = token
        self.timeout = timeout
        self.sleep = sleep

    @staticmethod
    def repeatable(method, path):
        return method in ("GET", "DELETE") or path.endswith("/statuses")

    def request(self, method, path, payload=None):
        delays = self.RETRY_DELAYS if self.repeatable(method, path) else ()
        for delay in delays:
            try:
                return self._send(method, path, payload)
            except _ServerError as error:
                print(f"{error}; retrying in {delay}s", flush=True)
                self.sleep(delay)
        return self._send(method, path, payload)

    def _send(self, method, path, payload):
        data = None if payload is None else json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            self.base + path, data=data, method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "Content-Type": "application/json",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                body = response.read().decode("utf-8")
                return json.loads(body) if body else None
        except urllib.error.HTTPError as error:
            body = error.read().decode("utf-8", errors="replace")
            kind = _ServerError if error.code >= 500 else RuntimeError
            raise kind(f"GitHub API {method} {path} failed: {error.code} {body}") from error


class _ServerError(RuntimeError):
    """A 5xx from GitHub: transient, so repeatable requests try again."""


def environment_for(session_id):
    """The stable deployment environment for one scenario."""
    safe_session = "".join(
        character if character.isalnum() or character in "-_." else "-"
        for character in session_id
    )[:64]
    return f"ggpo-e2e-{safe_session}"


class DeploymentRendezvous:
    """Publish, discover, and retire one exact workflow-run endpoint record."""

    def __init__(self, transport, repository, sha, run_id, attempt, session_id):
        self.transport = transport
        self.repository = repository
        self.sha = sha
        self.run_id = str(run_id)
        self.attempt = str(attempt)
        self.session_id = session_id
        # One environment per scenario, the same in every run, so a large
        # matrix cannot hide a peer record behind unrelated scenarios and the
        # repository's environments do not grow with each run (the workflow
        # token cannot delete environments). Runs sharing an environment are
        # told apart by commit, run and attempt in the payload, and each run's
        # final job deletes its deployments (rendezvous_cleanup.py).
        self.environment = environment_for(session_id)

    def publish(self, address, control_port):
        record = {
            "repository": self.repository,
            "sha": self.sha,
            "run_id": self.run_id,
            "attempt": self.attempt,
            "session_id": self.session_id,
            "address": address,
            "control_port": int(control_port),
        }
        deployment = self.transport.request("POST", "/deployments", {
            "ref": self.sha,
            "environment": self.environment,
            "description": "GGPO native E2E rendezvous",
            "auto_merge": False,
            "required_contexts": [],
            "transient_environment": True,
            "production_environment": False,
            "payload": record,
        })
        deployment_id = deployment["id"]
        self.transport.request("POST", f"/deployments/{deployment_id}/statuses", {
            "state": "in_progress",
            "environment": self.environment,
            "description": "Endpoint available",
        })
        return deployment_id, record

    def _matching(self, deployments):
        matches = []
        for deployment in deployments:
            payload = deployment.get("payload") or {}
            expected = {
                "repository": self.repository, "sha": self.sha,
                "run_id": self.run_id, "attempt": self.attempt,
                "session_id": self.session_id,
            }
            if deployment.get("environment") == self.environment and all(
                    payload.get(key) == value for key, value in expected.items()):
                matches.append(deployment)
        if len(matches) > 1:
            raise ValueError("duplicate rendezvous deployments")
        return matches

    def discover(self, deadline_seconds=120, poll_seconds=2):
        deadline = time.monotonic() + deadline_seconds
        query = urllib.parse.urlencode({"environment": self.environment,
                                        "ref": self.sha, "per_page": 100})
        while time.monotonic() < deadline:
            matches = self._matching(
                self.transport.request("GET", f"/deployments?{query}")
            )
            if matches:
                return matches[0]["id"], matches[0]["payload"]
            time.sleep(poll_seconds)
        raise TimeoutError("rendezvous deployment was not published before deadline")

    def retire(self, deployment_id):
        return self.transport.request(
            "POST", f"/deployments/{deployment_id}/statuses",
            {"state": "inactive", "environment": self.environment,
             "description": "Endpoint retired"},
        )

    def delete(self, deployment_id):
        """Delete an inactive deployment and its transient environment."""
        return self.transport.request("DELETE", f"/deployments/{deployment_id}")
