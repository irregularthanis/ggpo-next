# PROGRESS — Android support

Status: In progress

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Feasibility (spikes run in CI)

- [x] M0-T01: arm64 Android emulator on `ubuntu-24.04-arm`: KVM availability,
  boot time, `adb shell` executable run.
- [x] M0-T02: x86_64 emulator on `ubuntu-24.04` with KVM as fallback.
- [x] M0-T03: NDK build of the unmodified tree; record every failure.
- [x] M0-T04: Inventory of Android-specific differences in the platform layer.

### M1: Build (depends: M0)

- [x] M1-T01: `platform/android/Platform.cmake` selected for `Android`.
- [x] M1-T02: Share the POSIX adapter between Linux and Android.
- [ ] M1-T03: Android-only behavior isolated in the platform layer (logcat).
- [ ] M1-T04: Both libraries for arm64-v8a, armeabi-v7a, x86_64; `-ffp-contract=off`.
- [ ] M1-T05: Existing platforms unchanged (all current required checks pass).

### M2: Tests (depends: M1)

- [x] M2-T01: `adb` wrapper as `CMAKE_CROSSCOMPILING_EMULATOR`.
- [x] M2-T02: Full CTest suite passes on Android (arm64 preferred).
- [x] M2-T03: Frozen Vector War checksum matches on arm64.
- [ ] M2-T04: Installed consumers (default, static, shared) on Android.
- [ ] M2-T05: Layout and export manifest tests on every ABI; SDK audit
  (SONAME, API-level ceiling).
- [ ] M2-T06: Toolchain isolation and portable-header checks for Android.
- [ ] M2-T07: Android Debug lane and scheduled sanitizer job.
- [x] M2-T08: Tests on the oldest supported API (21) and a current API.
- [ ] M2-T09: 16 KB page alignment check on every `libGGPO.so`.

### M3: Vector War (depends: M1)

Parallel with M2.

- [ ] M3-T01: SDL2 frontend on Android with touch controls.
- [ ] M3-T02: Gradle APK using the same CMake.
- [ ] M3-T03: APK smoke run on the emulator in CI.

### M4: E2E (depends: M2)

- [ ] M4-T01: Android peer bundle and emulator role jobs with UDP forwarding.
- [ ] M4-T02: Android ↔ Linux, Windows and MinGW scenarios, smoke and Vector War.

### M5: Release, docs and parity close-out (depends: M3, M4)

- [ ] M5-T00: Every row of the [parity matrix](TECH_PLAN.md#parity-matrix)
  is done or has a written "not applicable" reason.

- [ ] M5-T01: Android SDK archive and Vector War APK; release dry run.
- [ ] M5-T02: Developer Guide parity: install, build from source, use with
  CMake, Gradle and without CMake, shared vs static, deployment, platform notes,
  and running Vector War; VectorWar.md.
- [ ] M5-T05: CI exercises the documented Android build and consumer commands.
- [ ] M5-T03: Build.md, Testing.md, CrossPlatformTesting.md, README.
- [ ] M5-T04: Android checks added to the ruleset and branch protection.

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| M0-T01 | [Spike run 42966490968](https://github.com/irregularthanis/ggpo-next/actions/runs/42966490968) (`3a28a73`), probe job | `ubuntu-24.04-arm` has **no `/dev/kvm`**: arm64 Android system images exist but no hosted arm64 emulator. Full-Android checks (dynamic `libGGPO.so`, APK) need the x86_64 emulator on `ubuntu-24.04` |
| M0-T01 (alt) | Same run, static execution | NDK `-static` arm64 executables (bionic linked in) run natively on the arm64 runner: 15/16 binaries pass; peer loopback reaches 180 frames with canonical checksum 2109130363, equal to Linux/Windows peers |
| M0-T03 | Same run, build job | Unmodified tree only failed platform selection (`Unsupported GGPO platform: Android`); POSIX adapter compiles unchanged with NDK 30 |
| FP | Same run: `vectorwar_state_contract` checkpoint 60 expected 3529340798, got 1851563108 | ARM FMA contraction; fixed by `-ffp-contract=off` on every Android target (`562f1e2`) |
| M0-T04 | Source inventory | No Android-specific source change needed so far; only build selection, unversioned `libGGPO.so`, and the FP flag |
| M1-T01, M1-T02 | `8f4fd11`; [Linux](https://github.com/irregularthanis/ggpo-next/actions/runs/42966143288), [MinGW](https://github.com/irregularthanis/ggpo-next/actions/runs/42966143117), [E2E](https://github.com/irregularthanis/ggpo-next/actions/runs/42966143136) | `platform_linux.cpp` → `platform_posix.cpp` shared by Linux and Android; existing platforms' CI passes, including the frozen Windows baseline |
| M2 (partial) | [Android compatibility 42966850489](https://github.com/irregularthanis/ggpo-next/actions/runs/42966850489) (`562f1e2`) | Full CTest 22/22 on arm64-v8a and **armeabi-v7a (32-bit EABI5, executed natively on the arm64 runner)** and x86_64; Vector War frozen checksum matches on all three |
| M0-T02, M2-T01, M2-T02, M2-T08 | [Android compatibility 42967759434](https://github.com/irregularthanis/ggpo-next/actions/runs/42967759434) (`89ebc6f`) | x86_64 emulator with KVM on API **21** and **35**: full CTest 25/25 each against the shared `libGGPO.so` through `tests/android/run_on_device.py` (including `android_exports`, schema-2 loopback with the live completion barrier, negative contracts, static runtime smokes); LastTest.log confirms on-device execution. Native arm64-v8a, armeabi-v7a and x86_64 jobs 22/22 |
| Launcher fix | `89ebc6f` | API 21 exposed that device-written ready files were pulled once; they are now pulled every cycle and host-written files pushed on change |

## Decisions

| Decision | Status |
|---|---|
| Same CMake build architecture; Android only in the platform layer | Accepted (2026-11-11) |
| Test and E2E parity with existing platforms; arm64 runners preferred | Accepted (2026-11-11) |
| Compatibility with existing platforms is a hard requirement | Accepted (2026-11-11) |
| Port Vector War (SDL2 APK + headless for E2E) | Accepted (2026-11-11) |
| Documentation parity: build, install, and use in apps | Accepted (2026-11-11) |
| Full parity with existing platforms, tracked in the parity matrix | Accepted (2026-11-11) |
| `-ffp-contract=off` on every Android target | Accepted (required: arm64 diverged without it) |
| Test runners: arm64-v8a and armeabi-v7a natively on `ubuntu-24.04-arm` (static executables), x86_64 natively on `ubuntu-24.04`; x86_64 emulator with KVM for full-Android checks | Accepted (M0 evidence) |
| `libc++_static` inside `libGGPO.so` | Proposed |
| Minimum API 21 (Android 5.0): floor only, newer devices unaffected; tested on API 21 and a current API | Accepted (2026-11-11) |
| Keep 32-bit `armeabi-v7a` alongside arm64-v8a and x86_64 | Accepted (2026-11-11) |
| Rename `platform_linux.cpp` to `platform_posix.cpp`, shared by Linux and Android | Accepted (2026-11-11), done in `8f4fd11` |
| NDK: newest LTS on the runners, 16 KB page aligned | Proposed (pinned in M1) |
