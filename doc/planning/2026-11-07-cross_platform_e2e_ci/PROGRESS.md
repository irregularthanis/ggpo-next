# PROGRESS — Native cross-platform end-to-end CI

Status: Planned

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze E2E contracts

Architecture: [native cross-host smoke](../../architecture/Testing.md#native-cross-host-smoke).

- [ ] M0-T01: Freeze schema 2, canonical byte order, independent oracle vectors, and schema 1 compatibility.
- [ ] M0-T02: Freeze run identity, HMAC message, Deployment rendezvous, permissions, trigger, port, timeout, and cleanup contracts.
- [ ] M0-T03: Record required secrets, Tailscale tag/grants, pinned action commit, and unavailable-evidence behavior.

### M1: Make fixture results cross-platform-safe (depends: M0)

Parallel with M2/M3; owns `tests/smoke` and schema/oracle code; integrates at M4.

- [ ] M1-T01: Add canonical state serialization and independent known-vector oracle without changing GGPO API or wire data.
- [ ] M1-T02: Extend peer/result/comparator schema and replace fixed-delay success with a bounded ready/finalize barrier.
- [ ] M1-T03: Pass legacy smoke/negative tests plus schema, oracle, completion, and wrong-result regressions.

### M2: Build authenticated orchestration (depends: M0)

Parallel with M1/M3; owns `tests/e2e`; integrates at M4.

- [ ] M2-T01: Implement platform-generated LL/WW/LW/WL matrix, allow-listed agents, HMAC messages, deadlines, and child cleanup.
- [ ] M2-T02: Prove the transient GitHub Deployment publish/discover/inactive lifecycle and bounded API behavior.
- [ ] M2-T03: Reject wrong run/SHA/session, bad MAC, replay, unsafe path/command, timeout, crash, and missing/duplicate result.

### M3: Wire native GitHub-hosted runners (depends: M0)

Parallel with M1/M2; owns `.github/workflows/cross-platform-e2e.yml`; integrates at M4.

- [ ] M3-T01: Build and hash native Linux/Windows Release x64 peer bundles from one commit.
- [ ] M3-T02: Join pinned ephemeral Tailscale nodes with least-privileged trusted-only triggers and grants.
- [ ] M3-T03: Run concurrent agents and unconditional artifact/Deployment/process cleanup; final report cannot mask failed jobs.

### M4: Qualify the clean native matrix (depends: M1, M2, M3)

- [ ] M4-T01: Pass LL, WW, LW, and WL with exactly 180 frames, canonical/oracle state, expected events, and clean closure.
- [ ] M4-T02: Pass three consecutive trusted runs with complete manifests, hashes, logs, summaries, and no orphan resources.
- [ ] M4-T03: Update testing/networking/portability docs and record workflow evidence before making the check required.

### M5: Add deterministic impairment (depends: M4)

- [ ] M5-T01: Add a seeded UDP proxy with recorded latency/loss decisions outside production GGPO.
- [ ] M5-T02: Run all four role assignments nightly under approved profiles and prove loads/replay frames occurred.
- [ ] M5-T03: Record reproducibility and tolerance findings without treating timing as a performance benchmark.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
|---|---|---|---|---|
