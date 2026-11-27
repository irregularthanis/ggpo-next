# PROGRESS — Disconnect before any input

Status: Planned

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Reproduce

- [ ] M0-T01: Peer negative contract: peer B exits on `RUNNING` before its
  first input; peer A must see the disconnect and continue. Fails on
  `master` with the `FindSavedFrameIndex` assert.
- [ ] M0-T02: Same through `ggpo_disconnect_player` before any input, and a
  three-player variant (`PollNPlayers`).
- [ ] M0-T03: `Sync` unit contract for a rollback to frame 0 after a
  prediction-only start.

### M1: Fix (depends: M0)

- [ ] M1-T01: Decide D1 (recommended: clamp the rollback target in
  `DisconnectPlayerQueue`).
- [ ] M1-T02: Implement; every M0 test passes; existing tests unchanged.
- [ ] M1-T03: All native CI lanes green, including sanitizers.

### M2: Release (depends: M1)

- [ ] M2-T01: Release note.
- [ ] M2-T02: Frame Perfect: drop `--known-crash` from
  `session_no_input_disconnect` when its migration reaches this version.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
| --- | --- | --- | --- | --- |
| Discovery | Frame Perfect `6d33b9d` | `session_no_input_disconnect` (sync-only proxy filter) | Linux, vendored GGPO | `ASSERT` in `sync.cpp` `FindSavedFrameIndex`, 3/3 runs |
