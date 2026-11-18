# PROGRESS — macOS support

Status: In progress (M1, M2 done)

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Decisions

- [x] M0-T01: D1 decided (2026-11-17): no Apple Developer account; unsigned
  packages.
- [x] M0-T02: D2 decided (2026-11-17): built for macOS 11, tested on macOS 15
  and 26 (the only GitHub-hosted versions).
- [x] M0-T03: D3 decided (2026-11-17): macOS is the public game's fourth
  player.

### M1: macOS library and CI

- [x] M1-T01: `src/platform/apple/Platform.cmake`, selected for Darwin.
- [x] M1-T02: `-ffp-contract=off` for Apple targets.
- [x] M1-T03: Universal static and shared builds for macOS 11;
  `platform_posix.cpp` compiles with `-Werror=unguarded-availability-new`.
- [x] M1-T04: Minimum-version audit (`minos 11.0`) and export audit.
- [x] M1-T05: CTest passes on macOS 15 and 26, Apple Silicon and Intel,
  including `vectorwar_state_contract`.
- [x] M1-T06: `macos-compatibility.yml` with its aggregate check, Xcode
  16.4 pinned.
- [x] M1-T07: `macos` area in `affected_areas.py`, with tests.

### M2: Release package (depends: M1)

- [x] M2-T01: macOS SDK archive, extracted and consumer-tested.

### M3: Matchmaking platform ID (parallel with M1–M2)

- [ ] M3-T01: ID 4 (macOS) in server, C++ codec and vectors.
- [ ] M3-T02: Legend color for macOS.
- [ ] M3-T03: Live server image updated (owner).

### M4: Vector War app (depends: M1, M3)

- [ ] M4-T01: `VectorWar.app` with SDL2.
- [ ] M4-T02: Unsigned app release package.

### M5: Live tests (depends: M4)

- [ ] M5-T01: macOS pairs in the cross-platform E2E matrix, capped for the
  macOS runner limit.
- [ ] M5-T02: macOS as the fourth player in the public multi-platform game.

### M6: Finish

- [ ] M6-T01: Ruleset requires the macOS aggregate check (owner).
- [ ] M6-T02: Developer guide, Testing.md and README updated.

## Log

- 2026-11-17: Plan drafted for macOS; iOS deferred. D1–D3 decided; D2 revised the same day to a macOS 11 baseline, tested on 15 and 26.
- 2026-11-18: M1 done. Universal build on macos-15 (Xcode 16.4): arm64 and x86_64 slices, minos 11.0 in libGGPO.dylib and libGGPO.a. 36/36 CTest tests pass on macOS 15 and 26, Apple Silicon and Intel, including `vectorwar_state_contract` and `macos_exports`. Every other workflow stayed green.
- 2026-11-18: M2 done. `package-macos` in `release-packages.yml` builds, tests, audits and archives `ggpo-<version>-macos-universal.tar.gz`, then runs the C11/C++98 consumers (default, static, shared) against the extracted archive; a manual dry run passed. The per-PR workflow runs the same consumers. DeveloperGuide gains a macOS section.
