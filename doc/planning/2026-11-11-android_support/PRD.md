# PRD — Android support

Status: Draft

Source: `a9407a88ffbd9d593806e6d520e00c3d3ad0b0f6`

Scope: build, test, package, and document the GGPO SDK and the Vector War
sample for Android, sharing the existing build architecture and as much code as
possible with Linux, Windows, and MinGW. Excluded: iOS/macOS, a Java/Kotlin API
binding (applications call the C API through the NDK), and any public API,
ABI, export, or wire-format change.

## Motivation

GGPO runs on Windows (MSVC, MinGW) and Linux. Android games and emulator front
ends cannot use it without carrying their own port. Android is a Linux kernel
with the Bionic C library and POSIX sockets, so the existing Linux platform
adapter already implements nearly everything GGPO needs. The work is mostly
build integration, test infrastructure that runs on Android, and packaging.

## Outcomes

- The SDK builds with the Android NDK through the existing CMake build for
  `arm64-v8a`, `armeabi-v7a`, and `x86_64`, producing both the shared and
  the static library with the same CMake package (`GGPO::GGPO`,
  `GGPO::shared`, `GGPO::static`, `GGPO_USE_STATIC`).
- Platform code is shared: Android uses the POSIX adapter that Linux uses, with
  only Android-specific differences isolated in the platform layer.
- **Test parity:** the full CTest suite (portable, API, protocol, platform
  conformance, SyncTest, peer loopback, Vector War contracts) runs on Android,
  preferably on arm64.
- **E2E parity:** Android peers play the generic peer fixture and Vector War
  against Linux, Windows MSVC, and MinGW peers in the native E2E matrix.
- **Vector War** runs on Android: the shared SDL2 frontend as an APK with
  on-screen controls, plus `VectorWarHeadless` for E2E.
- Releases add an Android SDK archive (all ABIs) and a Vector War APK.
- **Documentation parity:** the Developer Guide covers Android at the same depth
  as Linux, Windows and MinGW: how to **build** the SDK from source, how to
  **install** a release SDK, and how to **use** it in an application (CMake,
  Gradle, and without CMake), with shared and static linking and deployment.

## Success and constraints

| Metric | Baseline | Target | Validation |
|---|---|---|---|
| Android build | Fails: `Unsupported GGPO platform: Android` | Both libraries for arm64-v8a, armeabi-v7a, x86_64 | CI per ABI |
| Shared platform code | n/a | Android adds no copy of the POSIX adapter; Android-only code is limited to the platform layer | Code review; source inventory |
| CTest on Android | None | Same suite as Linux passes, on arm64 when a runner allows it | CI |
| E2E | Linux, Windows, MinGW | Android ↔ Linux/Windows/MinGW, smoke and Vector War, converge | E2E manifest |
| Vector War | Windows, Linux | Android APK starts, connects, and plays a two-player session | CI smoke + E2E with headless |
| Documentation | No Android content | Every Developer Guide topic that exists for Linux/Windows/MinGW has an Android equivalent; commands in the guide are exercised by CI | Doc checklist; CI runs the documented build and consumer commands |
| Compatibility | Frozen ABI/wire baseline; all current lanes | Unchanged; existing platforms' checks and packages identical | Existing required checks and frozen baseline |

## Constraints (hard)

- **Compatibility with existing platforms is mandatory.** No change to
  `ggponet.h`, exports, struct layout, calling convention, or wire format.
  Linux, Windows MSVC and MinGW builds, names, packages, and required checks
  must not change behavior. The frozen Windows baseline remains authoritative.
- Android peers must interoperate with every existing platform over the
  network, with identical deterministic results for the shared fixtures.
- Keep the build architecture: CMake with a platform-selected
  `Platform.cmake`, the same targets, exports, tests, and consumer checks. No
  separate Android-only build system for the SDK.
- Heavy builds and tests run in CI, not on developer machines.

## Users and stories

- **Android game developer:** "I add GGPO to my NDK/CMake project with
  `find_package(GGPO)` and choose shared or static, as on other platforms."
- **Cross-platform game:** "My Android build plays against Windows and Linux
  builds without desyncs."
- **Maintainer:** "Android regressions fail PR CI, and the existing platforms are
  unaffected."

## Risks

| Risk | Mitigation |
|---|---|
| No arm64 Android emulator on hosted runners | M0 spike: arm64 hosted runner first; fallback x86_64 emulator with KVM for PRs plus scheduled arm64 runs on real devices |
| ARM floating-point contraction (FMA) desyncs deterministic simulations | Compile GGPO, fixtures, and Vector War with `-ffp-contract=off` on Android; verify the frozen Vector War checksum on arm64; document it for games |
| Emulator NAT blocks inbound UDP for E2E | Emulator console `redir add udp:...` from the runner's tailnet address into the emulator |
| Android-only code drifts into the shared core | Platform-layer-only rule; inventory in M0; reviewed per milestone |
| CI time grows | Per-ABI builds are small; emulator image cached; ARM device runs scheduled if needed |
