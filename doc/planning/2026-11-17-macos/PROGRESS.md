# PROGRESS — macOS support

Status: Not started

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Decisions

- [ ] M0-T01: D1 — Apple Developer account, yes or no.
- [ ] M0-T02: D2 — minimum version (proposed macOS 11).
- [ ] M0-T03: D3 — macOS as the public game's fourth player.

### M1: macOS library and CI

- [ ] M1-T01: `src/platform/apple/Platform.cmake`, selected for Darwin.
- [ ] M1-T02: `-ffp-contract=off` for Apple targets.
- [ ] M1-T03: Universal static and shared builds; `platform_posix.cpp`
  compiles.
- [ ] M1-T04: Native CTest passes on arm64 and under Rosetta 2, including
  `vectorwar_state_contract`.
- [ ] M1-T05: `macos-compatibility.yml` with its aggregate check, Xcode
  pinned.
- [ ] M1-T06: `macos` area in `affected_areas.py`, with tests.

### M2: Release package (depends: M1)

- [ ] M2-T01: macOS SDK archive, extracted and consumer-tested.

### M3: Matchmaking platform ID (parallel with M1–M2)

- [ ] M3-T01: ID 4 (macOS) in server, C++ codec and vectors.
- [ ] M3-T02: Legend color for macOS.
- [ ] M3-T03: Live server image updated (owner).

### M4: Vector War app (depends: M1, M3)

- [ ] M4-T01: `VectorWar.app` with SDL2.
- [ ] M4-T02: App release package; signed and notarized if D1.

### M5: Live tests (depends: M4)

- [ ] M5-T01: macOS pairs in the cross-platform E2E matrix, capped for the
  macOS runner limit.
- [ ] M5-T02: macOS as the fourth player in the public multi-platform game.

### M6: Finish

- [ ] M6-T01: Ruleset requires the macOS aggregate check (owner).
- [ ] M6-T02: Developer guide, Testing.md and README updated.

## Log

- 2026-11-17: Plan drafted for macOS; iOS deferred.
