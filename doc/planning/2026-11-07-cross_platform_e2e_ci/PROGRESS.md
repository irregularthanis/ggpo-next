# PROGRESS — Native cross-platform end-to-end CI

Status: In progress

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze E2E contracts

Architecture: [native cross-host smoke](../../architecture/Testing.md#native-cross-host-smoke).

- [x] M0-T01: Freeze schema 2, canonical byte order, independent oracle vectors, and schema 1 compatibility.
- [x] M0-T02: Freeze run identity, HMAC message, Deployment rendezvous, permissions, trigger, port, timeout, and cleanup contracts.
- [x] M0-T03: Record required secrets, Tailscale tag/grants, pinned action commit, and unavailable-evidence behavior.

### M1: Make fixture results cross-platform-safe (depends: M0)

Parallel with M2/M3; owns `tests/smoke` and schema/oracle code; integrates at M4.

- [x] M1-T01: Add canonical state serialization and independent known-vector oracle without changing GGPO API or wire data.
- [x] M1-T02: Extend peer/result/comparator schema and replace fixed-delay success with a bounded ready/finalize barrier.
- [x] M1-T03: Pass legacy smoke/negative tests plus schema, oracle, completion, and wrong-result regressions.

### M2: Build authenticated orchestration (depends: M0)

Parallel with M1/M3; owns `tests/e2e`; integrates at M4.

- [x] M2-T01: Implement platform-generated LL/WW/LW/WL matrix, allow-listed agents, HMAC messages, deadlines, and child cleanup.
- [x] M2-T02: Prove the transient GitHub Deployment publish/discover/inactive lifecycle and bounded API behavior.
- [x] M2-T03: Reject wrong run/SHA/session, bad MAC, replay, unsafe path/command, timeout, crash, and missing/duplicate result.

### M3: Wire native GitHub-hosted runners (depends: M0)

Parallel with M1/M2; owns `.github/workflows/cross-platform-e2e.yml`; integrates at M4.

- [x] M3-T01: Build and hash native Linux/Windows Release x64 peer bundles from one commit.
- [x] M3-T02: Join pinned ephemeral Tailscale nodes with least-privileged trusted-only triggers and grants.
- [x] M3-T03: Run concurrent agents and unconditional artifact/Deployment/process cleanup; final report cannot mask failed jobs.

All required secrets are configured and two consecutive owner-only native runs
passed. M4 qualification is at 2/3 consecutive clean runs.

### M4: Qualify the clean native matrix (depends: M1, M2, M3)

- [x] M4-T01: Pass LL, WW, LW, and WL with exactly 180 frames, canonical/oracle state, expected events, and clean closure.
- [ ] M4-T02: Pass three consecutive trusted runs with complete manifests, hashes, logs, summaries, and no orphan resources.
- [ ] M4-T03: Update testing/networking/portability docs and record workflow evidence before making the check required.

### M5: Add deterministic impairment (depends: M4)

- [ ] M5-T01: Add a seeded UDP proxy with recorded latency/loss decisions outside production GGPO.
- [ ] M5-T02: Run all four role assignments nightly under approved profiles and prove loads/replay frames occurred.
- [ ] M5-T03: Record reproducibility and tolerance findings without treating timing as a performance benchmark.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
|---|---|---|---|---|
| M0-T01 | [`result_schema.py`](../../../tests/e2e/result_schema.py), [technical plan](TECH_PLAN.md#contracts-and-flow) | Source contract and fixed-vector review | Schema 1 and schema 2 | Frozen |
| M0-T02 | [`control.py`](../../../tests/e2e/control.py), [`rendezvous.py`](../../../tests/e2e/rendezvous.py), [workflow](../../../.github/workflows/cross-platform-e2e.yml) | Source contract review | Run/SHA/attempt/session-bound control | Frozen |
| M0-T03 | [Technical plan security contract](TECH_PLAN.md#workflow-and-security) | Permission, trigger, secret, action-pin review | Owner-authored same-repository PR, trusted `push`, or manual workflow | Frozen |
| M1-T01 | [`smoke_session.cpp`](../../../tests/smoke/smoke_session.cpp), [`test_result_schema.py`](../../../tests/e2e/test_result_schema.py) | `python3 tests/e2e/test_result_schema.py` | 9 schema/oracle cases | PASS |
| M1-T02 | [`peer_smoke.cpp`](../../../tests/smoke/peer_smoke.cpp), [`run_schema2_loopback.py`](../../../tests/e2e/run_schema2_loopback.py) | `linux_peer_schema2_loopback` | 180-frame bounded barrier | PASS |
| M1-T03 | `/tmp/ggpo-pr5-build` | `cmake -S . -B /tmp/ggpo-pr5-build -G Ninja -DCMAKE_BUILD_TYPE=Release`; build; `ctest --test-dir /tmp/ggpo-pr5-build --output-on-failure -C Release` | Linux Release; 9 registered tests | PASS, 9/9 (run twice: build-time check and explicit CTest) |
| M2-T01 | [`run_matrix.py`](../../../tests/e2e/run_matrix.py), [`control.py`](../../../tests/e2e/control.py), [`peer_agent.py`](../../../tests/e2e/peer_agent.py) | Source review plus `test_orchestration.py` | Four ordered platform roles; authenticated allow-list; bounded supervisor lifecycle | PASS |
| M2-T02 | [Deployment `7659683890`](https://api.github.com/repos/irregularthanis/ggpo-next/deployments/7659683890), [`test_rendezvous.py`](../../../tests/e2e/test_rendezvous.py) | Publish, discover exact payload by environment/ref, then mark inactive | Transient non-production record bound to repository, SHA, run, attempt, and session | PASS; payload visible and record retired |
| M2-T03 | [`test_orchestration.py`](../../../tests/e2e/test_orchestration.py), [`test_rendezvous.py`](../../../tests/e2e/test_rendezvous.py) | `python3 tests/e2e/test_orchestration.py`; `python3 tests/e2e/test_rendezvous.py` | 22 focused control, matrix, manifest, lifecycle, identity, and failure cases | PASS, 22/22 |
| M3-T01 | [Native E2E run `42723244592`](https://github.com/irregularthanis/ggpo-next/actions/runs/42723244592) | Native Release x64 Linux/Windows build jobs; SHA-256 and toolchain artifacts | PR merge SHA `9d8a562c4c27d5b573844b9edb5fe6590766f97d` | PASS |
| M3-T02 | [Eight native role jobs](https://github.com/irregularthanis/ggpo-next/actions/runs/42723244592) | Pinned Tailscale action; `tag:ggpo-ci`; TCP 24871 and UDP 24872–24873 | Four ephemeral scenario pairs | PASS |
| M3-T03 | [Complete manifest artifact](https://github.com/irregularthanis/ggpo-next/actions/runs/42723244592/artifacts/13721662369) | Final fail-closed report plus unconditional action/process/Deployment cleanup | 11 evidence artifacts; four Deployment records | PASS; all Deployments inactive |
| M4-T01 | [Complete manifest artifact](https://github.com/irregularthanis/ggpo-next/actions/runs/42723244592/artifacts/13721662369) | Validate all role reports against the independent oracle | LL, WW, LW, WL; 180 frames each | PASS; checksum `2109130363`, clean closure, no disconnects |
| M4-T02 (2/3) | [Native E2E run `42723789745`](https://github.com/irregularthanis/ggpo-next/actions/runs/42723789745), [manifest](https://github.com/irregularthanis/ggpo-next/actions/runs/42723789745/artifacts/13721219229) | Repeat the complete clean matrix and verify cleanup | Second consecutive LL/WW/LW/WL run; four Deployments | PASS; manifest complete and all Deployments inactive |
