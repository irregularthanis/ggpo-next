# TECH_PLAN — iOS support

Status: Draft

[Product brief](PRD.md) · [Progress](PROGRESS.md)

## Contents

- [Verified baseline](#verified-baseline)
- [Build host](#build-host)
- [Library build](#library-build)
- [Determinism](#determinism)
- [Vector War](#vector-war)
- [Matchmaking platform ID](#matchmaking-platform-id)
- [Testing](#testing)
- [CI](#ci)
- [Packaging and release](#packaging-and-release)
- [Adding macOS later](#adding-macos-later)
- [Documentation](#documentation)
- [Rollout](#rollout)
- [Risks](#risks)

## Verified baseline

Facts from `ee465d1` that constrain the design:

- `src/CMakeLists.txt` selects `platform/{windows,linux,android}/Platform.cmake`
  and stops with `Unsupported GGPO platform` for anything else, including
  `iOS`.
- `src/lib/ggpo/platform_posix.cpp` is shared by Linux and Android. It uses BSD
  sockets, `SO_REUSEADDR` and `clock_gettime(CLOCK_MONOTONIC)`, all available
  on iOS 10+. It has not been compiled for Apple yet.
- `-ffp-contract=off` is applied project-wide only `if(ANDROID)`. Apple clang
  contracts `a*b+c` into FMA on arm64 by default, which broke Vector War's
  checksum on Android arm64.
- Vector War's simulation takes cos/sin from a fixed table (`vw_trig.h`), so
  maths-library differences no longer reach the state.
- Android runs host-built tests on an emulator through `GGPO_TEST_LAUNCHER`
  (`tests/android/run_on_device.py`).
- The matchmaking protocol knows platform IDs 0–3 (unknown, Windows, Linux,
  Android); the live server image is pinned by digest and updated by hand.
- `release-packages.yml` builds Linux, Windows (MSVC, MinGW) and Android SDKs
  and the Android APK; each archive is extracted and checked before upload.
- The Free plan runs at most 5 macOS jobs at once (20 jobs in total).

## Build host

- All iOS jobs run on `macos-15` runners with the runner's default Xcode,
  pinned with `xcode-select` to a named version so a runner image update does
  not change the toolchain silently.
- No macOS target is built. The host's own compiler is used only for host
  tools (CMake, Python), never for GGPO.

## Library build

- New `src/platform/apple/Platform.cmake`, selected for `CMAKE_SYSTEM_NAME`
  `iOS` (and later `Darwin`, see [Adding macOS later](#adding-macos-later)).
  It compiles `platform_posix.cpp` with `GGPO_PLATFORM_APPLE` and
  `-Wall -Wextra`, like Android.
- Fix whatever the first iOS compile finds in `platform_posix.cpp` behind
  `#if defined(__APPLE__)`; expected candidates are header differences.
- CMake's built-in iOS support: `-G Xcode -DCMAKE_SYSTEM_NAME=iOS`,
  `CMAKE_OSX_SYSROOT=iphoneos|iphonesimulator`,
  `CMAKE_OSX_DEPLOYMENT_TARGET=14.0`. Device: arm64. Simulator: arm64 and
  x86_64.
- Static only (`BUILD_SHARED_LIBS=OFF`); a shared iOS build is rejected with a
  clear configure error.
- Export audit: the installed library's exported symbols must match
  `tests/baseline/linux/exports.txt`, checked with `nm -gU` like the Android
  audit.

## Determinism

- Extend the root `add_compile_options(-ffp-contract=off)` from `ANDROID` to
  `ANDROID OR APPLE`, with the comment updated.
- `vectorwar_state_contract` (2, 3 and 4 players, golden checksums) runs in
  the simulator and must pass unchanged. This is the first gate: an iOS build
  that disagrees with Linux fails here before any network test.

## Vector War

- An iOS app target built from the SDL2 frontend (SDL2 built from source as a
  static library through CMake, as the Android APK does), with the existing
  touch controls and start screen.
- `Info.plist`: `NSLocalNetworkUsageDescription` (needed for LAN direct play
  and hole punching on iOS 14+), launch screen, icons, landscape orientation.
- App lifecycle: on `SDL_APP_WILLENTERBACKGROUND` the session is closed
  cleanly and the app returns to the start screen when reopened.
- `VectorWarHeadless` builds as a simulator command-line executable for tests.

## Matchmaking platform ID

- Add `PLATFORM_IOS = 4` to `protocol.py`, the C++ codec and
  `protocol_vectors.json`; the legend gets a new color.
- Older servers report unknown IDs as "unknown" and still match players, so
  clients ship before the server. The live server image is updated by hand
  (as decided for the pinned image), after which the legend names iOS.

## Testing

- New `tests/ios/run_on_simulator.py`, the `GGPO_TEST_LAUNCHER` for iOS:
  boots or reuses a simulator (`xcrun simctl`), then runs the test executable
  with `xcrun simctl spawn <device> <path>`, forwarding `GGPO_*` and
  `VW_HEADLESS_*` variables and the exit code. The simulator shares the Mac's
  network, so loopback, matchmaking and E2E tests work as on desktop.
- CTest is configured with `CMAKE_CROSSCOMPILING_EMULATOR` set to the launcher
  so the existing `add_test` commands run unchanged.
- Coverage limit: the simulator runs the same arm64 code on Apple Silicon, but
  not on iPhone hardware. Real-device testing is a non-goal.

### Cross-platform E2E

- Add an `ios` build to `cross-platform-e2e.yml`, and pairs in both roles
  against Linux, Windows and Android (6 pairs, 12 jobs). Roles stay adjacent
  (checked by `test_e2e_matrix.py`).
- iOS pairs get their own matrix job with `max-parallel: 4`, so they never
  take all 5 macOS slots.
- Tailscale's GitHub Action supports macOS runners; the simulator reaches the
  tailnet through the host.

### Public multi-platform game

- iOS joins as player 4 (simulator on a macOS runner). The harness validator
  expects four platforms.

## CI

- `ios-compatibility.yml` on `macos-15`: build for device and simulator, CTest
  in the simulator, install, export audit; aggregate check
  "Complete iOS compatibility".
- `affected_areas.py`: new area `ios`; `platform_posix.cpp` maps to Linux,
  Android and iOS; `src/platform/apple/*` and `tests/ios/*` to iOS; the new
  workflow to its area.
- Ruleset 26776893: add the aggregate check as required (needs the owner;
  done after the workflow is green on `master`).
- Concurrency and cancel-in-progress as in the other workflows.

## Packaging and release

New jobs in `release-packages.yml` on `macos-15`:

- `package-ios`: build device and simulator static libraries, combine with
  `xcodebuild -create-xcframework -library … -headers …`, zip; validate by
  extracting and linking a minimal consumer for the simulator.
- `package-vectorwar-ios`: simulator `.app`, zipped with
  `ditto -c -k --keepParent`. A device build is produced only when signing is
  available (D1).
- Signing (only if D1 is yes): certificate and provisioning profile as
  repository secrets, imported into a temporary keychain. Without D1 the
  packages are unsigned and the release notes say how to use them.

## Adding macOS later

The pieces this plan builds make a macOS target small: `Platform.cmake`
already handles Apple, `-ffp-contract=off` already covers `APPLE`, and the
runners and Xcode pinning exist. A macOS step would add `Darwin` selection,
universal (`arm64;x86_64`) static and shared builds, native CTest, a workflow,
an SDK archive, and optionally the Vector War app bundle and E2E pairs.

## Documentation

- Developer guide: an iOS subsection (build, test, package), same structure as
  the other platforms.
- Testing.md: the simulator launcher and the iOS rows in the tables.
- README: supported platforms list and release assets.

## Rollout

Each step is one PR, with these documents updated in the same PR.

1. iOS library build (`Platform.cmake`, `-ffp-contract=off` for Apple),
   simulator launcher, CTest in the simulator, `ios-compatibility.yml`, area
   routing.
2. Release package: XCFramework.
3. Matchmaking platform ID for iOS (server, codecs, vectors, legend).
4. Vector War iOS app and its simulator package.
5. iOS pairs in the cross-platform E2E matrix.
6. iOS as the fourth player in the public game.
7. Ruleset update (owner), then documentation pass.

## Risks

- **Unknown POSIX differences** in `platform_posix.cpp` on Apple. Found by
  step 1's first compile; expected to be small.
- **macOS runner limit (5).** iOS pairs and release jobs queue; capped
  matrices keep them from blocking each other.
- **Simulator boot time** (1–2 minutes) adds to every iOS job; one simulator
  is booted per job and reused by all tests.
- **Xcode updates** on runner images; pinned with `xcode-select`.
- **SDL2 on iOS** needs a CMake build from source; Android's approach is
  reused.
- **Unsigned apps** can't be installed on phones without Xcode until D1 is
  decided.
