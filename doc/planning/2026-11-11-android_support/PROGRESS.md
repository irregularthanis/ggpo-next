# PROGRESS — Android support

Status: Planning

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Feasibility (spikes run in CI)

- [ ] M0-T01: arm64 Android emulator on `ubuntu-24.04-arm`: KVM availability,
  boot time, `adb shell` executable run.
- [ ] M0-T02: x86_64 emulator on `ubuntu-24.04` with KVM as fallback.
- [ ] M0-T03: NDK build of the unmodified tree; record every failure.
- [ ] M0-T04: Inventory of Android-specific differences in the platform layer.

### M1: Build (depends: M0)

- [ ] M1-T01: `platform/android/Platform.cmake` selected for `Android`.
- [ ] M1-T02: Share the POSIX adapter between Linux and Android.
- [ ] M1-T03: Android-only behavior isolated in the platform layer (logcat).
- [ ] M1-T04: Both libraries for arm64-v8a, armeabi-v7a, x86_64; `-ffp-contract=off`.
- [ ] M1-T05: Existing platforms unchanged (all current required checks pass).

### M2: Tests (depends: M1)

- [ ] M2-T01: `adb` wrapper as `CMAKE_CROSSCOMPILING_EMULATOR`.
- [ ] M2-T02: Full CTest suite passes on Android (arm64 preferred).
- [ ] M2-T03: Frozen Vector War checksum matches on arm64.
- [ ] M2-T04: Installed consumers (default, static, shared) on Android.

### M3: Vector War (depends: M1)

Parallel with M2.

- [ ] M3-T01: SDL2 frontend on Android with touch controls.
- [ ] M3-T02: Gradle APK using the same CMake.
- [ ] M3-T03: APK smoke run on the emulator in CI.

### M4: E2E (depends: M2)

- [ ] M4-T01: Android peer bundle and emulator role jobs with UDP forwarding.
- [ ] M4-T02: Android ↔ Linux, Windows and MinGW scenarios, smoke and Vector War.

### M5: Release and docs (depends: M3, M4)

- [ ] M5-T01: Android SDK archive and Vector War APK; release dry run.
- [ ] M5-T02: Developer Guide Android section; VectorWar.md.
- [ ] M5-T03: Build.md, Testing.md, CrossPlatformTesting.md, README.
- [ ] M5-T04: Android checks added to the ruleset and branch protection.

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| — | — | — |

## Decisions

| Decision | Status |
|---|---|
| Same CMake build architecture; Android only in the platform layer | Accepted (2026-11-11) |
| Test and E2E parity with existing platforms; arm64 runners preferred | Accepted (2026-11-11) |
| Compatibility with existing platforms is a hard requirement | Accepted (2026-11-11) |
| Port Vector War (SDL2 APK + headless for E2E) | Accepted (2026-11-11) |
| `-ffp-contract=off` on Android | Proposed |
| `libc++_static` inside `libGGPO.so` | Proposed |
| Minimum API 24, NDK r27 LTS, ABIs arm64-v8a/armeabi-v7a/x86_64 | Proposed |
