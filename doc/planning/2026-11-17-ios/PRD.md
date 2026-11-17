# PRD — iOS support

Status: Draft

Source: `ee465d1` (`master`)

Scope: build, test, package and release GGPO for iOS, and run Vector War on
iPhone and iPad, with CI coverage equal to the existing platforms. macOS is
used only as the build and test host, which Apple's toolchain requires.
Excluded: shipping GGPO or Vector War for macOS (it can be added later on the
same build setup); any GGPO public API, ABI, export or wire format change;
tvOS, watchOS and visionOS; App Store distribution; real-device testing in CI.

[Technical plan](TECH_PLAN.md) · [Progress](PROGRESS.md)

## Motivation

GGPO runs on Windows, Linux and Android, and Vector War plays across all three
over the Internet. iPhone players cannot join a match. iOS is a POSIX system,
so the existing POSIX adapter is expected to carry most of the port; the work
is mainly build configuration, determinism settings, a simulator test
launcher, CI and packaging.

### Why macOS appears at all

The iOS SDK, the iOS Simulator and `xcodebuild` ship only with Xcode, which
Apple licenses for Apple hardware. Every iOS build, test and package therefore
runs on GitHub's macOS runners. macOS is not a supported GGPO platform in this
plan.

## Outcomes

### Library

- GGPO builds for iOS from the same CMake project: device (arm64) and
  simulator (arm64 and x86_64) static libraries, with the same public header
  and exports as Linux.
- A Vector War match between iOS and any other platform stays bit-identical:
  iOS builds follow the same floating-point rules as Android (no fused
  multiply-add).

### Vector War

- An iOS app built from the SDL2 frontend, with the existing touch controls,
  start screen, matchmaking and legend. It joins matchmaking and direct
  sessions.
- The legend and the matchmaking server know iOS as a platform, with its own
  color.

### Testing

- iOS runs the full CTest suite inside the iOS Simulator through a test
  launcher, as Android runs its suite on an emulator.
- iOS takes part in the cross-platform E2E matrix in both roles against Linux,
  Windows and Android.
- The public multi-platform game adds iOS as the fourth player (GGPO's
  maximum).

### CI

- One workflow, `ios-compatibility.yml`, with an aggregate
  "Complete iOS compatibility" check, added to the ruleset like the others.
- Area routing sends iOS-only changes to the iOS workflow and core changes
  everywhere.
- macOS runners are limited to 5 concurrent jobs on the Free plan; iOS jobs
  are capped so they queue instead of starving each other.

### Packaging and release

- Each release adds:
  - `ggpo-<version>-ios.xcframework.zip`: an XCFramework with the device and
    simulator static libraries and the public header, usable from Xcode and
    Swift Package Manager;
  - `VectorWar-<version>-ios-simulator.zip`: the Vector War app for the
    simulator.
- Every archive is extracted and checked in CI before upload, as the existing
  packages are.
- Packages are unsigned. Signing needs an Apple Developer account (see open
  decisions).

## Non-goals

- macOS as a shipped platform (SDK, app, E2E pairs). Later, if wanted, it
  reuses this plan's Apple build setup.
- App Store or TestFlight distribution.
- Running tests on a physical iPhone in CI.
- Keeping an iOS app connected in the background: iOS suspends its sockets,
  and the match treats that as a disconnect.

## Open decisions

- **D1 — Apple Developer account** (USD 99/year). Without it: unsigned
  packages; the app installs on a phone only through Xcode with a free Apple
  ID (7-day expiry). With it: signed device builds, TestFlight possible.
- **D2 — Minimum version.** Proposed: iOS 14 (first version with the local
  network permission).
- **D3 — iOS in the public game.** Proposed: yes, as the fourth player; the
  match then takes longer to assemble and uses a macOS runner.
