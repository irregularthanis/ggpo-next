# PROGRESS — iOS support

Status: Not started

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Decisions

- [ ] M0-T01: D1 — Apple Developer account, yes or no.
- [ ] M0-T02: D2 — minimum version (proposed iOS 14).
- [ ] M0-T03: D3 — iOS as the public game's fourth player.

### M1: iOS library and CI

- [ ] M1-T01: `src/platform/apple/Platform.cmake`, selected for iOS.
- [ ] M1-T02: `-ffp-contract=off` for Apple targets.
- [ ] M1-T03: Device and simulator static builds; `platform_posix.cpp`
  compiles.
- [ ] M1-T04: `tests/ios/run_on_simulator.py` launcher.
- [ ] M1-T05: CTest passes in the simulator, including
  `vectorwar_state_contract`.
- [ ] M1-T06: `ios-compatibility.yml` with its aggregate check, Xcode pinned.
- [ ] M1-T07: `ios` area in `affected_areas.py`, with tests.

### M2: Release package (depends: M1)

- [ ] M2-T01: XCFramework archive, extracted and linked by a consumer.

### M3: Matchmaking platform ID (parallel with M1–M2)

- [ ] M3-T01: ID 4 (iOS) in server, C++ codec and vectors.
- [ ] M3-T02: Legend color for iOS.
- [ ] M3-T03: Live server image updated (owner).

### M4: Vector War app (depends: M1, M3)

- [ ] M4-T01: iOS app with touch controls, local network permission and
  background handling.
- [ ] M4-T02: Simulator app release package; device build if D1.

### M5: Live tests (depends: M4)

- [ ] M5-T01: iOS pairs in the cross-platform E2E matrix, capped for the
  macOS runner limit.
- [ ] M5-T02: iOS as the fourth player in the public multi-platform game.

### M6: Finish

- [ ] M6-T01: Ruleset requires the iOS aggregate check (owner).
- [ ] M6-T02: Developer guide, Testing.md and README updated.

## Log

- 2026-11-17: Plan drafted. macOS dropped as a shipped platform; macOS
  runners are the build and test host only.
