# PROGRESS — Shared-core testing and CI consolidation

Status: Planned

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze coverage and CI baselines

Architecture: [test layers](../../architecture/Testing.md#test-layers).

- [ ] M0-T01: Inventory all 16 public exports and classify existing coverage.
- [ ] M0-T02: Inventory portable components and all 15 platform operations.
- [ ] M0-T03: Record current tests, compatibility gates, job count, runner minutes,
  critical path, cache behavior and retry/failure evidence.
- [ ] M0-T04: Prioritize missing contracts without removing any test or lane.

### M1: Add deterministic core contracts (depends: M0)

Parallel with M2/M3; owns `tests/unit`; integrates in M5.

- [ ] M1-T01: Test InputQueue ordering, prediction, mismatch, delay and wraparound.
- [ ] M1-T02: Test Sync snapshot, rollback, replay, ownership and prediction limits.
- [ ] M1-T03: Test TimeSync histories and container/bit-vector boundaries.
- [ ] M1-T04: Prove independent expected results and exercised rollback paths.

### M2: Add public API contracts (depends: M0)

Parallel with M1/M3; owns `tests/contract`; integrates in M5.

- [ ] M2-T01: Classify backend/lifecycle/error behavior for every export.
- [ ] M2-T02: Add creation, registration, input and frame-lifecycle contracts.
- [ ] M2-T03: Add close/disconnect/stats/config/logging and cleanup contracts.
- [ ] M2-T04: Keep unspecified inputs distinct from stable compatibility behavior.

### M3: Build reusable platform conformance (depends: M0)

Parallel with M1/M2; owns `tests/platform`; integrates in M5.

- [ ] M3-T01: Extend only the fake capabilities required by concrete regressions.
- [ ] M3-T02: Implement one mandatory suite for all 16 callable platform
  operations and socket construction/destruction lifecycle.
- [ ] M3-T03: Run identical assertions against Windows and Linux adapters.
- [ ] M3-T04: Reject silent mandatory skips and verify bounded cleanup failures.

### M4: Harden protocol contracts (depends: M1, M3)

- [ ] M4-T01: Preserve frozen packet layout and document compiler/endian assumptions.
- [ ] M4-T02: Add length/type/payload boundary and supported malformed-input cases.
- [ ] M4-T03: Prove no public ABI or production wire-format change.

### M5: Qualify platform portability (depends: M1, M2, M3, M4)

- [ ] M5-T01: Define the supported/experimental adapter checklist.
- [ ] M5-T02: Preserve Windows ABI/DLL and Linux ABI/package requirements.
- [ ] M5-T03: Prove a future adapter reuses core, API and conformance suites.
- [ ] M5-T04: Publish platform limitations and native qualification evidence.

### M6: Consolidate CI from evidence (depends: M1, M2, M3, M4, M5)

- [ ] M6-T01: Label portable/native/ABI suites while preserving existing CTest names.
- [ ] M6-T02: Add correctly instrumented Linux ASan/UBSan and manual/full tiers.
- [ ] M6-T03: Demonstrate replacement coverage before reducing regular PR variants.
- [ ] M6-T04: Compare jobs, runner minutes, critical path, cache and retry rate.
- [ ] M6-T05: Preserve full release, Windows ABI, linkage and consumer qualification.

### M7: Integrate documentation and E2E boundary (depends: M3, M5)

- [ ] M7-T01: Update Testing, Build and Portability with verified commands/targets.
- [ ] M7-T02: Cross-reference PR #5’s peer artifacts, result schema and ownership.
- [ ] M7-T03: Document future-platform E2E qualification without duplicating its harness.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
|---|---|---|---|---|
