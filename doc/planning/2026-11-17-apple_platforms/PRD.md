# PRD — macOS and iOS support

Status: Draft

Source: `ee465d1` (`master`)

Scope: build, test, package and release GGPO for macOS and iOS, and run Vector
War on both, with CI coverage equal to the existing platforms. Excluded: any
GGPO public API, ABI, export or wire format change; tvOS, watchOS and visionOS;
App Store distribution; real-device testing in CI.

[Technical plan](TECH_PLAN.md) · [Progress](PROGRESS.md)

## Motivation

GGPO runs on Windows, Linux and Android, and Vector War plays across all three
over the Internet. Apple platforms are the remaining large gap: iPhone and Mac
players cannot join a match. Both are POSIX systems, so the existing POSIX
adapter is expected to carry most of the port; the work is mainly build
configuration, determinism settings, test launchers, CI and packaging.

macOS comes first. It runs tests natively, needs no simulator or signing, and
settles the Apple compiler, flags and runners before iOS adds a simulator and
an app bundle.

## Outcomes

### Library

- GGPO builds for macOS (arm64 and x86_64, one universal binary) and iOS
  (device arm64; simulator arm64 and x86_64) from the same CMake project.
- macOS gets a static library and a shared library (`libGGPO.dylib`); iOS gets
  a static library. Both expose the same public header and exports as Linux.
- A Vector War match between any two platforms stays bit-identical: Apple
  builds follow the same floating-point rules as Android (no fused
  multiply-add).

### Vector War

- **macOS:** the SDL2 frontend runs as a normal app, with the start screen,
  matchmaking and legend; the headless peer runs from the command line.
- **iOS:** an app built from the SDL2 frontend with the existing touch controls
  and start screen. It joins matchmaking and direct sessions.
- The legend and the matchmaking server know two new platforms, macOS and iOS,
  with their own colors.

### Testing

- macOS runs the full CTest suite natively, like Linux.
- iOS runs the same suite inside the iOS Simulator through a test launcher,
  as Android runs its suite on an emulator.
- macOS and iOS take part in the cross-platform E2E matrix in both roles
  against Linux, Windows and Android.
- The public multi-platform game adds iOS as the fourth player (GGPO's maximum).

### CI

- One workflow per platform (`macos-compatibility.yml`, `ios-compatibility.yml`)
  with an aggregate "Complete …" check, added to the ruleset like the others.
- Area routing sends Apple-only changes to the Apple workflows and core changes
  everywhere.
- macOS runners are limited to 5 concurrent jobs on the Free plan; Apple jobs
  are capped so they queue instead of starving each other.

### Packaging and release

- Each release adds:
  - `ggpo-<version>-macos.tar.gz`: installed CMake SDK, universal static and
    shared libraries;
  - `ggpo-<version>-ios.xcframework.zip`: an XCFramework with the device and
    simulator static libraries and the public header, usable from Xcode and
    Swift Package Manager;
  - `VectorWar-<version>-macos.zip`: the Vector War app bundle;
  - `VectorWar-<version>-ios-simulator.zip`: the Vector War app for the
    simulator.
- Every archive is extracted and checked in CI before upload, as the existing
  packages are.
- Packages are unsigned. Signing and notarization are optional and need an
  Apple Developer account (see open decisions).

## Non-goals

- App Store or TestFlight distribution.
- Running tests on a physical iPhone or Mac in CI.
- Keeping an iOS app connected in the background: iOS suspends its sockets,
  and the match treats that as a disconnect.

## Open decisions

- **D1 — Apple Developer account** (USD 99/year). Without it: unsigned
  packages; macOS users clear the quarantine flag to open Vector War; iOS
  installs only through Xcode with a free Apple ID (7-day expiry). With it:
  signed and notarized macOS app, signed iOS builds, TestFlight possible.
- **D2 — Minimum versions.** Proposed: macOS 11 and iOS 14 (first versions
  with the local network permission and arm64 Macs).
- **D3 — iOS in the public game.** Proposed: yes, as the fourth player; the
  match then takes longer to assemble and uses a macOS runner.
