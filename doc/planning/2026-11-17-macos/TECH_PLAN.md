# TECH_PLAN — macOS support

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
- [Adding iOS later](#adding-ios-later)
- [Documentation](#documentation)
- [Rollout](#rollout)
- [Risks](#risks)

## Verified baseline

Facts from `ee465d1` that constrain the design:

- `src/CMakeLists.txt` selects `platform/{windows,linux,android}/Platform.cmake`
  and stops with `Unsupported GGPO platform` for anything else, including
  `Darwin`.
- `src/lib/ggpo/platform_posix.cpp` is shared by Linux and Android. It uses BSD
  sockets, `SO_REUSEADDR` and `clock_gettime(CLOCK_MONOTONIC)`, all available
  on macOS 10.12+. It has not been compiled for Apple yet.
- `-ffp-contract=off` is applied project-wide only `if(ANDROID)`. Apple clang
  contracts `a*b+c` into FMA on arm64 by default, which broke Vector War's
  checksum on Android arm64.
- Vector War's simulation takes cos/sin from a fixed table (`vw_trig.h`), so
  maths-library differences no longer reach the state.
- The matchmaking protocol knows platform IDs 0–3 (unknown, Windows, Linux,
  Android); the live server image is pinned by digest and updated by hand.
- `release-packages.yml` builds Linux, Windows (MSVC, MinGW) and Android SDKs
  and the Android APK; each archive is extracted and checked before upload.
- The Free plan runs at most 5 macOS jobs at once (20 jobs in total).

## Build host

- All macOS jobs run on `macos-15` runners (Apple Silicon), with Xcode pinned
  through `xcode-select` to a named version so a runner image update does not
  change the toolchain silently.
- Universal binaries are built on the arm64 runner; the x86_64 slice is
  tested by running the test suite under Rosetta 2 as well (`arch -x86_64`),
  so both slices are executed, not only compiled.

## Library build

- New `src/platform/apple/Platform.cmake`, selected for `CMAKE_SYSTEM_NAME`
  `Darwin` (and later `iOS`, see [Adding iOS later](#adding-ios-later)). It
  compiles `platform_posix.cpp` with `GGPO_PLATFORM_APPLE` and `-Wall -Wextra`,
  like Android.
- Fix whatever the first macOS compile finds in `platform_posix.cpp` behind
  `#if defined(__APPLE__)`; expected candidates are header differences.
- `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
  `CMAKE_OSX_DEPLOYMENT_TARGET=15.0` (D2). Shared and static builds as on Linux;
  the dylib gets `@rpath/libGGPO.dylib` as its install name.
- Export audit: the installed library's exported symbols must match
  `tests/baseline/linux/exports.txt`, checked with `nm -gU` for both slices.

## Determinism

- Extend the root `add_compile_options(-ffp-contract=off)` from `ANDROID` to
  `ANDROID OR APPLE`, with the comment updated.
- `vectorwar_state_contract` (2, 3 and 4 players, golden checksums) runs on
  both slices and must pass unchanged. This is the first gate: a macOS build
  that disagrees with Linux fails here before any network test.

## Vector War

- The SDL2 frontend builds as a `MACOSX_BUNDLE` app (`VectorWar.app`) with an
  `Info.plist` and icon. SDL2 comes from Homebrew in CI builds and is built
  from source as a universal framework for the release package.
- `VectorWarHeadless` builds as a plain command-line tool for tests.
- macOS asks the player to allow incoming network connections the first time
  an unsigned app listens; direct and hole-punched play need that, relay play
  does not.

## Matchmaking platform ID

- Add `PLATFORM_MACOS = 4` to `protocol.py`, the C++ codec and
  `protocol_vectors.json`; the legend gets a new color.
- Older servers report unknown IDs as "unknown" and still match players, so
  clients ship before the server. The live server image is updated by hand
  (as decided for the pinned image), after which the legend names macOS.

## Testing

- Native CTest, same suite as Linux. `GGPO_POSIX_TEST_PREFIX` becomes `macos`.
- Both slices: CTest runs natively (arm64) and again under Rosetta 2
  (x86_64).

### Cross-platform E2E

- Add a `macos` build to `cross-platform-e2e.yml`, and pairs in both roles
  against Linux, Windows and Android (6 pairs, 12 jobs). Roles stay adjacent
  (checked by `test_e2e_matrix.py`).
- macOS pairs get their own matrix job with `max-parallel: 4`, so they never
  take all 5 macOS slots.
- Tailscale's GitHub Action supports macOS runners.

### Public multi-platform game

- macOS joins as player 4 (D3). The harness validator expects four
  platforms.

## CI

- `macos-compatibility.yml` on `macos-15`: universal build (static and
  shared), CTest on both slices, install, export audit, installed-consumer
  test; aggregate check "Complete macOS compatibility".
- `affected_areas.py`: new area `macos`; `platform_posix.cpp` maps to Linux,
  Android and macOS; `src/platform/apple/*` to macOS; the new workflow to its
  area.
- Ruleset 26776893: add the aggregate check as required (needs the owner;
  done after the workflow is green on `master`).
- Concurrency and cancel-in-progress as in the other workflows.

## Packaging and release

New jobs in `release-packages.yml` on `macos-15`:

- `package-macos`: universal Release build, `cmake --install` to a stage,
  export audit, `tar.gz`; validate by extracting and building a small consumer
  with `find_package(GGPO)` against both the static and the shared library.
- `package-vectorwar-macos`: `VectorWar.app` with SDL2 embedded in
  `Contents/Frameworks` (`@rpath`), zipped with `ditto -c -k --keepParent`;
  validate by extracting and launching the headless peer from the bundle.
- Unsigned (D1): no signing or notarization. Release notes say to open the
  app the first time with right-click → Open.

## Adding iOS later

The Apple build setup carries over: `Platform.cmake` already handles Apple and
`-ffp-contract=off` already covers `APPLE`. iOS would add `iOS` selection,
static device and simulator builds, a simulator test launcher
(`xcrun simctl spawn`, like Android's emulator launcher), an XCFramework
package, an SDL2 app target with the local network permission, and its own
platform ID.

## Documentation

- Developer guide: a macOS subsection (build, test, package), same structure
  as the other platforms.
- Testing.md: the macOS rows in the tables.
- README: supported platforms list and release assets.

## Rollout

Each step is one PR, with these documents updated in the same PR.

1. macOS library build (`Platform.cmake`, `-ffp-contract=off` for Apple),
   native CTest on both slices, `macos-compatibility.yml`, area routing.
2. Release package: macOS SDK.
3. Matchmaking platform ID for macOS (server, codecs, vectors, legend).
4. Vector War macOS app and its package.
5. macOS pairs in the cross-platform E2E matrix.
6. macOS as the fourth player in the public game.
7. Ruleset update (owner), then documentation pass.

## Risks

- **Unknown POSIX differences** in `platform_posix.cpp` on Apple. Found by
  step 1's first compile; expected to be small.
- **macOS runner limit (5).** macOS pairs and release jobs queue; capped
  matrices keep them from blocking each other.
- **Rosetta 2 on runners.** If a runner image lacks it, the x86_64 slice is
  only compiled and audited, not executed; the workflow installs it when
  missing.
- **Xcode updates** on runner images; pinned with `xcode-select`.
- **Unsigned app** needs one extra step to open (D1).
