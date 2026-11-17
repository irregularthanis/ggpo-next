# PRD — macOS support

Status: Draft

Source: `ee465d1` (`master`)

Scope: build, test, package and release GGPO for macOS, and run Vector War on
Macs, with CI coverage equal to the existing platforms. Excluded: iOS (it can
be added later on the same Apple build setup); any GGPO public API, ABI,
export or wire format change; Mac App Store distribution.

[Technical plan](TECH_PLAN.md) · [Progress](PROGRESS.md)

## Motivation

GGPO runs on Windows, Linux and Android, and Vector War plays across all three
over the Internet. Mac players cannot join a match, and Mac developers have no
SDK. macOS is a POSIX system, so the existing POSIX adapter is expected to
carry most of the port; the work is mainly build configuration, determinism
settings, CI and packaging. Tests run natively, as on Linux.

## Outcomes

### Library

- GGPO builds for macOS from the same CMake project as one universal binary
  (arm64 and x86_64), as a static library and a shared library
  (`libGGPO.dylib`), with the same public header and exports as Linux.
- A Vector War match between a Mac and any other platform stays
  bit-identical: macOS builds follow the same floating-point rules as Android
  (no fused multiply-add).

### Vector War

- The SDL2 frontend runs as a normal Mac app, with the start screen,
  matchmaking and legend; the headless peer runs from the command line.
- The legend and the matchmaking server know macOS as a platform, with its own
  color.

### Testing

- macOS runs the full CTest suite natively, like Linux.
- macOS takes part in the cross-platform E2E matrix in both roles against
  Linux, Windows and Android.
- The public multi-platform game adds macOS as the fourth player (GGPO's
  maximum).

### CI

- One workflow, `macos-compatibility.yml`, with an aggregate
  "Complete macOS compatibility" check, added to the ruleset like the others.
- Area routing sends macOS-only changes to the macOS workflow and core changes
  everywhere.
- macOS runners are limited to 5 concurrent jobs on the Free plan; macOS jobs
  are capped so they queue instead of starving each other.

### Packaging and release

- Each release adds:
  - `ggpo-<version>-macos.tar.gz`: installed CMake SDK with universal static
    and shared libraries;
  - `VectorWar-<version>-macos.zip`: the Vector War app bundle with SDL2
    embedded.
- Every archive is extracted and checked in CI before upload, as the existing
  packages are.
- Packages are unsigned. Signing and notarization need an Apple Developer
  account (see open decisions).

## Non-goals

- iOS. Later, if wanted, it reuses this plan's Apple build setup and adds a
  simulator test launcher and an app target.
- Mac App Store distribution.

## Open decisions

- **D1 — Apple Developer account** (USD 99/year). Without it: unsigned
  packages; players open Vector War once through right-click → Open (or clear
  the quarantine flag), and the SDK works unchanged. With it: signed and
  notarized app that opens normally.
- **D2 — Minimum version.** Proposed: macOS 11 (first version on Apple Silicon
  Macs).
- **D3 — macOS in the public game.** Proposed: yes, as the fourth player; the
  match then uses a macOS runner.
