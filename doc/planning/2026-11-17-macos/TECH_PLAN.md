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

- Builds run on `macos-15` (Apple Silicon), with Xcode pinned through
  `xcode-select` to a named version so a runner image update does not change
  the toolchain silently.
- The universal build is tested on four runners: `macos-15`, `macos-26`
  (Apple Silicon, arm64 slice) and `macos-15-intel`, `macos-26-intel` (x86_64
  slice). Both slices run natively; no Rosetta.
- No GitHub-hosted runner offers macOS 11–14 (14 was retired on 2026-11-02),
  and macOS cannot run on non-Apple hardware such as OCI, so those versions
  are checked at build time only (see [Library build](#library-build)).

## Library build

- New `src/platform/apple/Platform.cmake`, selected for `CMAKE_SYSTEM_NAME`
  `Darwin` (and later `iOS`, see [Adding iOS later](#adding-ios-later)). It
  compiles `platform_posix.cpp` with `GGPO_PLATFORM_APPLE` and `-Wall -Wextra`,
  like Android.
- Fix whatever the first macOS compile finds in `platform_posix.cpp` behind
  `#if defined(__APPLE__)`; expected candidates are header differences.
- `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
  `CMAKE_OSX_DEPLOYMENT_TARGET=11.0` (D2). The first build confirms that the
  pinned Xcode still accepts 11.0 as a deployment target. Shared and static builds as on Linux;
  the dylib gets `@rpath/libGGPO.dylib` as its install name.
- `-Werror=unguarded-availability-new` on Apple targets: using an API newer
  than macOS 11 without an `@available`/`__builtin_available` check fails the
  build.
- Minimum-version audit: every installed binary (both slices) must declare
  macOS 11.0 as its minimum (`vtool -show-build` / `otool -l`, `minos 11.0`).
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
  `Info.plist`. SDL2 is built from pinned, checksummed source as a static
  universal macOS 11 library for CI and release alike; Homebrew's SDL2 is
  single-architecture and targets the build host's macOS.
- `VectorWarHeadless` builds as a plain command-line tool for tests.
- macOS asks the player to allow incoming network connections the first time
  an unsigned app listens; direct and hole-punched play need that, relay play
  does not.

## Matchmaking platform ID

- Add `PLATFORM_MACOS = 4` to `protocol.py`, the C++ codec and the legend
  (named "macOS"); tests cover the numbering and pass-through.
- The server copies platform bytes into `MATCH` without interpreting them, so
  the live server already relays ID 4 and needs no update. Legend colors are
  per ship, not per platform, so no new color is needed. The protocol
  vectors are unchanged: the wire format is the same.

## Testing

- Native CTest, same suite as Linux. `GGPO_POSIX_TEST_PREFIX` becomes `macos`.
- The suite runs on macOS 15 and 26, Apple Silicon and Intel (four jobs). The
  build job uploads the universal build; the test jobs only run it, so all
  four test the same binaries.
- macOS 11–14 are not run (see [Build host](#build-host)). An optional,
  non-required job on a self-hosted Intel Mac with older macOS VMs could be
  added later; it is not part of this plan.

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

- `macos-compatibility.yml`: universal build (static and shared) on
  `macos-15`, minimum-version and export audits, then CTest and an
  installed-consumer test on `macos-15`, `macos-26`, `macos-15-intel` and
  `macos-26-intel`; aggregate check "Complete macOS compatibility". Five macOS
  jobs, exactly the Free plan's macOS limit, so the test matrix uses
  `max-parallel: 2` and leaves room for other workflows.
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
- `package-vectorwar-macos`: `VectorWar.app` with SDL2 linked statically,
  on an unsigned `.dmg` with an Applications link; validate by mounting it,
  auditing the binary and running `--frontend-smoke` from the image.
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
  as the other platforms, stating "built for macOS 11 and later; tested on
  macOS 15 and 26".
- Testing.md: the macOS rows in the tables.
- README: supported platforms list and release assets.

## Rollout

Each step is one PR, with these documents updated in the same PR.

1. macOS library build (`Platform.cmake`, `-ffp-contract=off` for Apple,
   macOS 11 target and audits), CTest on macOS 15 and 26 (arm64 and Intel),
   `macos-compatibility.yml`, area routing.
2. Release package: macOS SDK.
3. Matchmaking platform ID for macOS (server constants, client codec, legend).
4. Vector War macOS app and its package.
5. macOS pairs in the cross-platform E2E matrix.
6. macOS as the fourth player in the public game.
7. Ruleset update (owner), then documentation pass.

## Risks

- **Unknown POSIX differences** in `platform_posix.cpp` on Apple. Found by
  step 1's first compile; expected to be small.
- **macOS runner limit (5).** macOS pairs and release jobs queue; capped
  matrices keep them from blocking each other.
- **macOS 11–14 untested at run time.** Build-time checks catch newer-API use
  but not OS behaviour differences (for example networking); reports from
  users on those versions are the only signal.
- **Intel runners** may be retired before Apple Silicon ones; the x86_64 slice
  then falls back to running under Rosetta 2 on Apple Silicon.
- **Xcode may drop macOS 11** as a deployment target; the baseline then rises
  to the lowest one the pinned Xcode accepts.
- **Xcode updates** on runner images; pinned with `xcode-select`.
- **Unsigned app** needs one extra step to open (D1).
