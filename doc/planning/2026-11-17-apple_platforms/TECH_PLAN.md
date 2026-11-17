# TECH_PLAN — macOS and iOS support

Status: Draft

[Product brief](PRD.md) · [Progress](PROGRESS.md)

## Contents

- [Verified baseline](#verified-baseline)
- [Library build](#library-build)
- [Determinism](#determinism)
- [Vector War](#vector-war)
- [Matchmaking platform IDs](#matchmaking-platform-ids)
- [Testing](#testing)
- [CI](#ci)
- [Packaging and release](#packaging-and-release)
- [Documentation](#documentation)
- [Rollout](#rollout)
- [Risks](#risks)

## Verified baseline

Facts from `ee465d1` that constrain the design:

- `src/CMakeLists.txt` selects `platform/{windows,linux,android}/Platform.cmake`
  and stops with `Unsupported GGPO platform` for anything else, including
  `Darwin` and `iOS`.
- `src/lib/ggpo/platform_posix.cpp` is shared by Linux and Android. It uses BSD
  sockets, `SO_REUSEADDR` and `clock_gettime(CLOCK_MONOTONIC)`, all available
  on macOS 10.12+ and iOS 10+. It has not been compiled for Apple yet.
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

## Library build

- New `src/platform/apple/Platform.cmake`, selected for
  `CMAKE_SYSTEM_NAME` `Darwin` and `iOS`. It compiles `platform_posix.cpp`
  with `GGPO_PLATFORM_APPLE` and `-Wall -Wextra`, like Android.
- Fix whatever the first Apple compile finds in `platform_posix.cpp` behind
  `#if defined(__APPLE__)`; expected candidates are `SO_NOSIGPIPE` (Apple has
  no `MSG_NOSIGNAL`) and header differences.
- macOS: `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
  `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`. Shared and static builds as on Linux;
  the dylib gets `@rpath/libGGPO.dylib` as its install name.
- iOS: CMake's built-in support (`-G Xcode -DCMAKE_SYSTEM_NAME=iOS`,
  `CMAKE_OSX_SYSROOT=iphoneos|iphonesimulator`, deployment target 14.0).
  Static only (`BUILD_SHARED_LIBS=OFF`); a shared build is rejected with a
  clear configure error.
- Export audit: the installed library's exported symbols must match
  `tests/baseline/linux/exports.txt`, checked with `nm -gU` like the Android
  audit.

## Determinism

- Extend the root `add_compile_options(-ffp-contract=off)` from `ANDROID` to
  `ANDROID OR APPLE`, with the comment updated.
- `vectorwar_state_contract` (2, 3 and 4 players, golden checksums) runs on
  both Apple platforms and must pass unchanged. This is the first gate: any
  Apple build that disagrees with Linux fails here before any network test.

## Vector War

### macOS

- The SDL2 frontend builds as a `MACOSX_BUNDLE` app (`VectorWar.app`) with an
  `Info.plist`; SDL2 comes from Homebrew in CI and from the SDL2 framework in
  the release package.
- `VectorWarHeadless` builds as a plain command-line tool.

### iOS

- An app target built from the SDL2 frontend (SDL2 built from source as a
  static library through CMake, as the Android APK does), with the existing
  touch controls and start screen.
- `Info.plist`: `NSLocalNetworkUsageDescription` (needed for LAN direct play
  and hole punching on iOS 14+), launch screen, icons, landscape orientation.
- App lifecycle: on `SDL_APP_WILLENTERBACKGROUND` the session is closed
  cleanly and the app returns to the start screen when reopened.
- `VectorWarHeadless` builds as a simulator command-line executable for tests.

## Matchmaking platform IDs

- Add `PLATFORM_MACOS = 4` and `PLATFORM_IOS = 5` to `protocol.py`, the C++
  codec and `protocol_vectors.json`; the legend gets two new colors.
- Older servers report unknown IDs as "unknown" and still match players, so
  clients ship before the server. The live server image is updated by hand
  (as decided for the pinned image), after which the legend names them.

## Testing

### macOS

- Native CTest, same suite as Linux. `GGPO_POSIX_TEST_PREFIX` becomes `macos`.

### iOS

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

- Add `macos` and `ios` builds to `cross-platform-e2e.yml`, and pairs in both
  roles: macOS and iOS against Linux, Windows and Android, plus macOS–iOS
  (16 pairs, 32 jobs). Roles stay adjacent (checked by
  `test_e2e_matrix.py`).
- macOS runners count against the 5-job macOS limit. Apple pairs get their
  own matrix job with `max-parallel: 4` so they never take all macOS slots.
- Tailscale's GitHub Action supports macOS runners; the simulator reaches the
  tailnet through the host.

### Public multi-platform game

- iOS joins as player 4 (simulator on a macOS runner). The harness validator
  expects four platforms.

## CI

- `macos-compatibility.yml`: build (universal, static and shared), CTest,
  install, export audit, installed-consumer test; aggregate check
  "Complete macOS compatibility". Runner: `macos-15`.
- `ios-compatibility.yml`: build for device and simulator, CTest in the
  simulator, export audit; aggregate check "Complete iOS compatibility".
- `affected_areas.py`: new areas `macos` and `ios`; `platform_posix.cpp` maps
  to Linux, Android, macOS and iOS; `src/platform/apple/*` to macOS and iOS;
  `tests/ios/*` to iOS; the new workflows to their areas.
- Ruleset 26776893: add the two aggregate checks as required (needs the
  owner; done after the workflows are green on `master`).
- Concurrency and cancel-in-progress as in the other workflows.

## Packaging and release

New jobs in `release-packages.yml` on `macos-15`:

- `package-macos`: universal Release build, `cmake --install` to a stage,
  export audit, `tar.gz`; validate by extracting and building a small consumer
  with `find_package(GGPO)`.
- `package-ios`: build device and simulator static libraries, combine with
  `xcodebuild -create-xcframework -library … -headers …`, zip; validate by
  extracting and linking a minimal Xcode consumer for the simulator.
- `package-vectorwar-macos`: `VectorWar.app` with SDL2 embedded
  (`Contents/Frameworks`, `@rpath`), zipped with `ditto -c -k --keepParent`.
- `package-vectorwar-ios`: simulator `.app`, zipped. A device build is only
  produced when signing is available (D1).
- Signing (only if D1 is yes): certificates and profiles as repository
  secrets, imported into a temporary keychain; macOS notarization through
  `notarytool`. Without D1 the packages are unsigned and the release notes
  explain how to open them.

## Documentation

- Developer guide: macOS and iOS subsections (build, test, package), same
  structure as the other platforms.
- Testing.md: the simulator launcher and the Apple rows in the tables.
- README: supported platforms list and release assets.

## Rollout

Each step is one PR, with these documents updated in the same PR.

1. macOS library build, `-ffp-contract=off` for Apple, native CTest, macOS
   workflow and area routing.
2. iOS library build and simulator launcher, iOS workflow.
3. Release packages for both SDKs.
4. Matchmaking platform IDs 4 and 5 (server, codecs, vectors, legend).
5. Vector War macOS app and package.
6. Vector War iOS app and package.
7. E2E pairs for macOS and iOS.
8. iOS as the fourth player in the public game.
9. Ruleset update (owner), then documentation pass.

## Risks

- **Unknown POSIX differences** in `platform_posix.cpp` on Apple. Found by
  step 1's first compile; expected to be small.
- **macOS runner limit (5).** Apple E2E pairs and release jobs queue; capped
  matrices keep them from blocking each other.
- **Simulator boot time** (1–2 minutes) adds to every iOS job; one simulator
  is booted per job and reused by all tests.
- **SDL2 on iOS** needs its Xcode project or CMake build; Android's approach
  (SDL2 from source) is reused.
- **Unsigned apps** are inconvenient for players until D1 is decided.
