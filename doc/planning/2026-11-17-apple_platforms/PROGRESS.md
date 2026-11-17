# PROGRESS — macOS and iOS support

Status: Not started

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Decisions

- [ ] M0-T01: D1 — Apple Developer account, yes or no.
- [ ] M0-T02: D2 — minimum versions (proposed macOS 11, iOS 14).
- [ ] M0-T03: D3 — iOS as the public game's fourth player.

### M1: macOS library and CI

- [ ] M1-T01: `src/platform/apple/Platform.cmake`, selected for Darwin and iOS.
- [ ] M1-T02: `-ffp-contract=off` for Apple targets.
- [ ] M1-T03: `platform_posix.cpp` compiles and passes on macOS.
- [ ] M1-T04: Native CTest passes, including `vectorwar_state_contract`.
- [ ] M1-T05: `macos-compatibility.yml` with its aggregate check.
- [ ] M1-T06: `macos` area in `affected_areas.py`, with tests.

### M2: iOS library and CI (depends: M1)

- [ ] M2-T01: iOS device and simulator static builds.
- [ ] M2-T02: `tests/ios/run_on_simulator.py` launcher.
- [ ] M2-T03: CTest passes in the simulator.
- [ ] M2-T04: `ios-compatibility.yml` with its aggregate check.
- [ ] M2-T05: `ios` area in `affected_areas.py`, with tests.

### M3: Release packages (depends: M1, M2)

- [ ] M3-T01: macOS SDK archive, extracted and consumer-tested.
- [ ] M3-T02: iOS XCFramework, extracted and linked by a consumer.

### M4: Matchmaking platform IDs (parallel with M1–M3)

- [ ] M4-T01: IDs 4 (macOS) and 5 (iOS) in server, C++ codec and vectors.
- [ ] M4-T02: Legend colors for macOS and iOS.
- [ ] M4-T03: Live server image updated (owner).

### M5: Vector War apps (depends: M1, M2, M4)

- [ ] M5-T01: macOS `VectorWar.app` with SDL2, and its release package.
- [ ] M5-T02: iOS app with touch controls, local network permission and
  background handling.
- [ ] M5-T03: iOS simulator app release package; device build if D1.

### M6: Live tests (depends: M5)

- [ ] M6-T01: macOS and iOS pairs in the cross-platform E2E matrix, capped
  for the macOS runner limit.
- [ ] M6-T02: iOS as the fourth player in the public multi-platform game.

### M7: Finish

- [ ] M7-T01: Ruleset requires the macOS and iOS aggregate checks (owner).
- [ ] M7-T02: Developer guide, Testing.md and README updated.

## Log

- 2026-11-17: Plan drafted.
