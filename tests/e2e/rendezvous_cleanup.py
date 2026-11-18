#!/usr/bin/env python3
"""Retire and delete every rendezvous deployment one workflow run created.

Role jobs delete their own record when they finish, but a cancelled or
crashed job never reaches that code. The workflow's final job, which also runs
on cancellation, calls this to remove whatever the run left behind. It only
touches rendezvous deployments (environment ``ggpo-e2e-*``) whose payload
names this exact run and attempt; environments are stable per scenario and
shared between runs, so the payload is what identifies the run.
"""

import argparse
import os
import sys
import urllib.parse

try:
    from .rendezvous import GitHubTransport
except ImportError:  # Direct script execution from tests/e2e.
    from rendezvous import GitHubTransport


def belongs_to_run(deployment, run_id, attempt):
    payload = deployment.get("payload") or {}
    return (str(deployment.get("environment", "")).startswith("ggpo-e2e-")
            and str(payload.get("run_id")) == str(run_id)
            and str(payload.get("attempt")) == str(attempt))


def leftovers(transport, sha, run_id, attempt):
    """This run's rendezvous deployments for the commit, all pages."""
    found = []
    page = 1
    while True:
        query = urllib.parse.urlencode({"ref": sha, "per_page": 100, "page": page})
        batch = transport.request("GET", f"/deployments?{query}") or []
        found += [d for d in batch if belongs_to_run(d, run_id, attempt)]
        if len(batch) < 100:
            return found
        page += 1


def cleanup(transport, sha, run_id, attempt):
    removed = 0
    failures = []
    for deployment in leftovers(transport, sha, run_id, attempt):
        identifier = deployment["id"]
        try:
            # Only inactive deployments can be deleted.
            transport.request("POST", f"/deployments/{identifier}/statuses", {
                "state": "inactive", "environment": deployment["environment"],
                "description": "Run finished; rendezvous removed"})
            transport.request("DELETE", f"/deployments/{identifier}")
            removed += 1
        except RuntimeError as error:
            failures.append(f"{identifier}: {error}")
    return removed, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--run-id", default=os.environ.get("GITHUB_RUN_ID"))
    parser.add_argument("--attempt", default=os.environ.get("GITHUB_RUN_ATTEMPT"))
    parser.add_argument("--sha", default=os.environ.get("GITHUB_SHA"))
    args = parser.parse_args()
    repository = os.environ["GITHUB_REPOSITORY"]
    transport = GitHubTransport(repository, os.environ["GITHUB_TOKEN"],
                                os.environ.get("GITHUB_API_URL", "https://api.github.com"))
    removed, failures = cleanup(transport, args.sha, args.run_id, args.attempt)
    print(f"removed {removed} rendezvous deployment(s) for run {args.run_id} "
          f"attempt {args.attempt}")
    for failure in failures:
        print(f"could not remove {failure}", file=sys.stderr)
    # Cleanup never fails the run: a leftover record is clutter, not a result.
    return 0


if __name__ == "__main__":
    sys.exit(main())
