#!/usr/bin/env python3
"""Run-scoped GitHub Deployment rendezvous for concurrent native runners."""

import json
import time
import urllib.error
import urllib.parse
import urllib.request


class GitHubTransport:
    """Minimal GitHub REST transport with bounded request timeouts."""

    def __init__(self, repository, token, api_url="https://api.github.com", timeout=15):
        self.base = f"{api_url.rstrip('/')}/repos/{repository}"
        self.token = token
        self.timeout = timeout

    def request(self, method, path, payload=None):
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
            raise RuntimeError(f"GitHub API {method} {path} failed: {error.code} {body}") from error


class DeploymentRendezvous:
    """Publish, discover, and retire one exact workflow-run endpoint record."""

    def __init__(self, transport, repository, sha, run_id, attempt, session_id):
        self.transport = transport
        self.repository = repository
        self.sha = sha
        self.run_id = str(run_id)
        self.attempt = str(attempt)
        self.session_id = session_id
        # GitHub returns only one page of deployments. Give every scenario its
        # own environment so a large matrix cannot hide an older peer record
        # behind newer, unrelated rendezvous deployments.
        safe_session = "".join(
            character if character.isalnum() or character in "-_." else "-"
            for character in session_id
        )[:64]
        self.environment = (
            f"ggpo-e2e-{self.run_id}-{self.attempt}-{safe_session}"
        )

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
                                        "ref": self.sha, "per_page": 10})
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
