# TECH_PLAN — Android support

Status: Planning only

Source: `a9407a88ffbd9d593806e6d520e00c3d3ad0b0f6`

[Product brief](PRD.md) · [Tracker](PROGRESS.md)

## Verified baseline

- [`src/CMakeLists.txt`](../../../src/CMakeLists.txt) selects
  `platform/windows` or `platform/linux` and fails for any other
  `CMAKE_SYSTEM_NAME`; the NDK reports `Android`.
- [`platform_linux.cpp`](../../../src/lib/ggpo/platform_linux.cpp) (252 lines)
  implements all 16 operations in [`platform.h`](../../../src/lib/ggpo/platform.h)
  with POSIX calls: `clock_gettime`, `poll`/`nanosleep`, `getpid`,
  `getenv`, `mkdir`, `inet_pton`, and non-blocking UDP sockets. Bionic
  provides all of them.
- [`tests/CMakeLists.txt`](../../../tests/CMakeLists.txt) names native tests
  by platform prefix (`linux_*`, `windows_*`) and already supports
  cross-compiled targets through `CMAKE_CROSSCOMPILING_EMULATOR`.
- Vector War has a portable core, `VectorWarHeadless`, and an SDL2 frontend
  ([`sdl_main.cpp`](../../../src/apps/vectorwar/sdl_main.cpp),
  [`sdl_host.cpp`](../../../src/apps/vectorwar/sdl_host.cpp),
  [`sdl_renderer.cpp`](../../../src/apps/vectorwar/sdl_renderer.cpp)).
- The native E2E harness distributes per-platform peer bundles, runs role jobs
  over Tailscale, and validates a fixed scenario manifest.

## Parity matrix

Every capability of the existing platforms, with its Android equivalent. M5
cannot close while any row is open.

| Area | Existing platforms | Android equivalent |
| --- | --- | --- |
| **Build** | One CMake build, `Platform.cmake` per platform | Same CMake; `platform/android/Platform.cmake` |
| Libraries | Shared + static from one build (`GGPO_BUILD_SHARED/STATIC`, `BUILD_SHARED_LIBS`) | Same, per ABI |
| CMake package | `GGPO::GGPO`/`shared`/`static`, `GGPO_USE_STATIC`, `COMPONENTS` | Same |
| Toolchain record | `GGPO_TOOLCHAIN` msvc/mingw/gnu, mismatch rejected on Windows | `android`; mismatch (non-Android consumer) rejected |
| Architectures | Windows x64/x86, Linux x86-64 | arm64-v8a, armeabi-v7a, x86_64 |
| Runtime embedding | MinGW DLL embeds libgcc/libstdc++ | `libGGPO.so` uses `libc++_static` |
| Determinism flags | 32-bit MinGW `-msse2 -mfpmath=sse` | `-ffp-contract=off` |
| **Tests** | Portable core, API, protocol, fake platform | Same suite, run on Android |
| | Native platform conformance (`*_platform_conformance`) | `android_platform_conformance` |
| | Public/packet layout (`*_public_layout`, `*_packet_layout`) | `android_public_layout`, `android_packet_layout` on every ABI (incl. 32-bit armv7) |
| | Export manifest (`linux_exports`, MinGW/MSVC manifests) | `android_exports`: `llvm-readelf --dyn-syms` vs the frozen 16-export manifest |
| | SyncTest, peer loopback, schema-2 loopback, negative contracts | `android_*` equivalents through the `adb` wrapper |
| | Static-library runtime smokes (`*_static_*`) | `android_static_synctest_smoke`, `android_static_peer_loopback` |
| | Vector War state/gameplay contracts, native host contract | Same; frozen checksum on arm64 |
| | Vector War headless loopback and negative contracts | Same, on device |
| | GUI host contract (Win32 / SDL) | `android_vectorwar_gui_host_contract` (SDL on Android) |
| | Frontend smoke (`--frontend-smoke`) | APK launched in smoke mode on the emulator |
| | Installed consumers: C11/C++98 × default/static/shared, runtime dependency check | Same via `verify_consumers.py` Android mode (`NEEDED` check) |
| | SDK audit (Linux SONAME, glibc ceiling, exports) | SONAME `libGGPO.so`, **API-level ceiling** (no symbol newer than `minSdk`), exports |
| | Toolchain isolation (MSVC vs MinGW) | Android SDK rejected by a host-Linux consumer and vice versa |
| | Portable public header (C11/C++98, gcc/clang) | Same header compiled with the NDK clang for each ABI |
| **Sanitizers** | Linux ASan+UBSan, MSVC ASan (scheduled) | NDK ASan/HWASan + UBSan on arm64 (scheduled) |
| **Debug coverage** | One Windows Debug lane | One Android Debug build + test lane (arm64) |
| **E2E** | Smoke + Vector War, every pairing in both roles | `android-{linux,windows,mingw}` and reverse, smoke and `vectorwar-*`, manifest requires all |
| **Release** | SDK zips/DEB/RPM/tar.gz, Vector War per platform, dry run, `SHA256SUMS` | Android SDK archive (all ABIs), Vector War APK, in the dry run and checksums |
| **Governance** | Required checks in ruleset + branch protection | Android build, test and isolation checks added |
| **Docs** | Developer Guide install/build/use, architecture docs, README | Same topics for Android (see Documentation) |
| Frozen MSVC baseline | Windows-only by definition | Not applicable: it freezes the historical Windows DLL. Android is held to the same frozen layout values and export manifest instead |

## Platform layer

Android reuses the POSIX adapter instead of copying it:

```mermaid
flowchart TD
    Core["Portable core<br/>(unchanged)"]
    API["platform.h<br/>16 operations"]
    Posix["platform_posix.cpp<br/>(today platform_linux.cpp)"]
    Win["platform_windows.cpp<br/>MSVC and MinGW"]
    LinuxCfg["platform/linux/Platform.cmake"]
    AndroidCfg["platform/android/Platform.cmake<br/>NDK flags, log library"]
    Core --> API
    API --> Posix
    API --> Win
    Posix --> LinuxCfg
    Posix --> AndroidCfg
```

- Rename `platform_linux.cpp` to `platform_posix.cpp` (`git mv`, no
  behavior change), or keep the name and include it from both platform files;
  decided in M1 by the size of the Android delta.
- Android-specific behavior, if any, stays behind `#ifdef __ANDROID__` inside
  the platform file or in a small `platform_android.cpp`:
  - `AssertFailed` also writes to logcat (`__android_log_print`).
  - Default log/config locations: Android apps have no writable working
    directory; `EnsureDirectory` callers get an app-provided path.
- `src/CMakeLists.txt` selects `platform/android` when
  `CMAKE_SYSTEM_NAME STREQUAL "Android"`. Everything else (targets, exports,
  `GGPOConfig.cmake`, toolchain record `GGPO_TOOLCHAIN = android`) is shared.
- Android builds compile with `-ffp-contract=off` (see Determinism).

## Builds

| ABI | Use | Minimum API |
|---|---|---|
| `arm64-v8a` | All current devices | 21 |
| `armeabi-v7a` | Older 32-bit devices, TV boxes | 21 |
| `x86_64` | Emulators, Chromebooks | 21 |

The newest NDK LTS on the runners (pinned in M1), `-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake
-DANDROID_ABI=<abi> -DANDROID_PLATFORM=21`, `libc++_static` for the shared
library so `libGGPO.so` carries no C++ runtime dependency (mirrors the MinGW
decision); the static library uses the application's STL.

## Tests on Android

Tests stay command-line executables; no test APK is needed for CTest.

- `tests/android/run_on_device.py` is set as `CMAKE_CROSSCOMPILING_EMULATOR`:
  it `adb push`es the executable (and `libGGPO.so`, `libc++_shared.so` if
  any) to `/data/local/tmp/ggpo/`, runs it with `adb shell`, and returns its
  exit code and output. `ctest` then runs the **same suite** as Linux.
- Python-driven tests (peer loopback runner, negative contracts) run on the host
  and start their peer executables through the same wrapper.
- Test prefix `android_*` for native tests, mirroring `linux_*`/`windows_*`.
- `verify_consumers.py` gains an Android mode: configure the consumers with the
  NDK against the installed SDK, push and run them, and check `NEEDED` entries
  with `llvm-readelf` for shared vs static.

## Runners (arm64 preferred)

| Priority | Runner | Runs | Status |
|---|---|---|---|
| 1 | GitHub `ubuntu-24.04-arm` with an arm64 Android emulator | arm64-v8a natively | **M0 spike**: needs KVM on the arm runner and a Linux arm64 emulator build |
| 2 | GitHub `ubuntu-24.04` with KVM and an x86_64 emulator (`reactivecircus/android-emulator-runner`) | x86_64 ABI | Known to work; PR fallback |
| 3 | Real arm64/armv7 devices (Firebase Test Lab or AWS Device Farm) through a small test APK | arm64-v8a, armeabi-v7a | Scheduled/pre-release if option 1 fails |

API coverage: tests run on the **oldest supported API (21)** and a current API
level, so the minimum is proven rather than assumed. GGPO uses only POSIX calls
present in API 21; any new call newer than 21 must be guarded or raise the
minimum through a recorded decision.

The required PR check runs on option 1 if the spike succeeds, otherwise option 2
with option 3 weekly. Emulator images and AVD snapshots are cached.

## Determinism

AArch64 and ARMv7 compilers contract `a*b+c` into fused multiply-add by
default, which rounds differently from x86. GGPO's core state is integer, but
Vector War and many games simulate with floating point. Android builds of GGPO,
the fixtures, and Vector War use `-ffp-contract=off`; the frozen Vector War
checksum (`b3381958` at frame 180) must match on arm64. The guide tells game
developers to do the same.

## Vector War on Android

- Reuse the SDL2 frontend: SDL2's Android support (`SDLActivity`) hosts
  `sdl_main.cpp` unchanged in structure; `SDLVectorWarHost` gains touch
  input mapped to the same input bits (on-screen D-pad and fire/bomb buttons).
- Command-line arguments come from a small launcher screen or intent extras
  (`local port`, `player count`, `local`/`IP:PORT` list).
- Gradle project under `src/apps/vectorwar/android/` builds the APK by
  calling the same CMake (`externalNativeBuild`); the SDK stays CMake-only.
- `VectorWarHeadless` is cross-compiled like the tests and used in E2E.
- CI: build the APK, install it on the emulator, launch it in a smoke mode
  (`--frontend-smoke` equivalent through an intent extra), and require a clean
  exit.

## E2E

- New peer bundle `e2e-peer-android`: `ggpo_peer_smoke` and
  `vectorwar_headless` for the runner's ABI.
- Android role jobs run on the emulator runner: the job joins the tailnet on the
  host, forwards the UDP ports with the emulator console
  (`redir add udp:<port>:<port>`), and starts the peer inside the emulator
  through the same `adb` wrapper. The control agent stays on the host.
- Scenarios (both fixtures): `android-linux`, `linux-android`,
  `android-windows`, `windows-android`, `android-mingw`, plus
  `vectorwar-android-*` equivalents. The manifest requires all of them.

## CI

| Workflow | Addition |
|---|---|
| New `android-compatibility.yml` | `Android build (<abi>)` for three ABIs; `Android tests (arm64 or x86_64)` full CTest + consumers |
| Cross-platform E2E | `Build native Android peer`; Android scenarios above |
| Release | `Android SDK` (all ABIs in one archive) and `Android Vector War` (APK) |
| Ruleset + branch protection | Add the Android build and test checks at merge |

Existing workflows are not modified except to add jobs.

## Packaging

```text
ggpo-V-android/
├── include/ggponet.h
├── <abi>/lib/libGGPO.so
├── <abi>/lib/libGGPO.a
├── <abi>/lib/cmake/GGPO/          # one CMake package per ABI
└── share/GGPO/
```

A Prefab AAR for Gradle consumers is a possible follow-up, not in scope.

## Documentation (parity with existing platforms)

Every Developer Guide topic that exists for Linux, Windows and MinGW gets an
Android equivalent:

| Topic | Existing platforms | Android content |
| --- | --- | --- |
| **Install a release SDK** | Zip / DEB / RPM / tar.gz, layout, checksums | Android archive, per-ABI layout, `SHA256SUMS`, where to extract it |
| **Build from source** | CMake on Windows, CI container on Linux, MSYS2 for MinGW | NDK + CMake commands per ABI, minimum API, CI container option, building all ABIs |
| **Use with CMake** | `find_package(GGPO)`, `GGPO_USE_STATIC` | Same, with the per-ABI `CMAKE_PREFIX_PATH`/`CMAKE_FIND_ROOT_PATH` an NDK build needs |
| **Use from Gradle** | n/a | `externalNativeBuild` + CMake, passing the SDK prefix per ABI |
| **Use without CMake** | `cl`/`g++` command lines | NDK `clang++` command lines for shared and static |
| **Shared vs static, deployment** | DLL next to the exe, RPATH | Packaging `libGGPO.so` in the APK (`jniLibs` / Gradle), static linking into the app's own `.so` |
| **Platform notes** | Windows `/MD`, MinGW runtimes, SSE2 | `INTERNET` permission, `-ffp-contract=off`, ABI selection, API level |
| **Run Vector War** | Windows zip, Linux tarball | Installing the APK, entering ports and peers, two devices or device + PC |

Build.md, Testing.md, CrossPlatformTesting.md, VectorWar.md and README are
updated to match. CI exercises the documented build and consumer commands, as
`verify_consumers.py` does today, so the guide cannot drift from what works.

## Rollout

1. M0 spikes: runner feasibility (arm64 emulator), NDK build of the unmodified tree.
2. Platform selection and shared POSIX adapter; existing CI unchanged.
3. Android CTest and consumers in CI.
4. Vector War APK.
5. E2E scenarios.
6. Release, docs, required checks.

## Open decisions

- NDK version: the newest LTS on the runners, pinned in M1; libraries must be
  16 KB page aligned (default in newer NDKs, `-Wl,-z,max-page-size=16384` on
  older ones) and CI checks it.
- Required test runner: arm64 emulator (if the spike succeeds) or x86_64
  emulator plus scheduled devices.
