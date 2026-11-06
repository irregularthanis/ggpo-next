# PRD — Platform-agnostic GGPO core and Windows build layer

Status: Draft  
Source: [`ggpo-next` master at `004ffd7`](https://github.com/irregularthanis/ggpo-next/tree/004ffd7f47603d3bae4abd1d541501d715e03279)  
Scope: Portable core/API boundaries and the first explicit platform layer for Windows; excludes Linux, Apple, mobile, console, and portable Vector War implementations.

## Motivation

GGPO's rollback algorithms are largely platform-independent, but the current SDK
mixes them with Win32 types, Winsock behavior, process lifecycle, polling, CRT
helpers, and build flags. That makes every new platform port edit the core and
risk changing the public contract.

The requested outcome is an additive porting model: preserve the supported
Windows SDK while defining stable core, API/ABI, and build boundaries that later
Linux and Apple PRs can implement without forking GGPO's behavior.

## Outcomes

- The public header has one C-compatible source contract, explicit export and
  calling-convention macros, and no direct Windows requirement for consumers.
- Existing Windows x64 public signatures, exported symbols, structure layouts,
  callback layouts, and runtime behavior remain compatible.
- Existing source consumers rebuild unchanged, and binaries linked to the M0
  shared-library ABI continue loading and running with the refactored DLL.
- Portable rollback, backend, and protocol code no longer names Win32 handles,
  socket errors, clocks, sleeps, filesystem calls, or secure-CRT functions.
- Windows-specific sources, definitions, system libraries, and packaging rules
  live in one build layer selected by the root build.
- The installed SDK exposes only public headers and a namespaced CMake target;
  internal headers stop leaking through the consumer interface.
- Platform-agnostic smoke tests run the core against a deterministic fake
  adapter; platform smoke tests run the same adapter contract against Windows.
- Native builds compile and execute their self-contained tests by default; a
  failed test fails the build rather than leaving validation as an optional step.
- Linux, Apple, and sample-frontend ports remain follow-ups against the frozen
  contracts. Unsupported platforms fail configuration with a useful message.

## Success and constraints

| Measure | Baseline | Target | Validation |
| --- | --- | --- | --- |
| Windows builds | AppVeyor builds x64 Debug/Release, static/shared | Same matrix remains green after separation | CMake build and install matrix |
| Public API | `extern "C"`, but `bool` and `__cdecl` are not portable C declarations | Header compiles as C and C++ without platform shims in consumer code | Installed-header compile/link fixtures |
| Windows ABI | No checked-in export/layout baseline | Export names and public type offsets/sizes are captured and unchanged | `dumpbin` export manifest plus layout assertions |
| Existing consumers | Current source and binaries target the Windows SDK directly | Unmodified source rebuilds; M0 consumer binaries run with the new DLL | Old/new header, import library, DLL, and executable compatibility matrix |
| Core boundary | `HANDLE`, `SOCKET`, WSA, Win32 wait/time, and secure CRT leak into core | No direct OS symbols outside the selected platform layer and Windows sample | Source-contract check plus builds |
| Consumer surface | Internal `src/lib/ggpo` is a public include directory | Only `src/include` is public; `GGPO::GGPO` installs and links | External consumer smoke project |
| Runtime behavior | No automated SDK regression suite | SyncTest and local UDP peers produce matching frame/checksum results | CTest integration evidence |
| Core smoke | Core cannot run without Win32-shaped facilities | Deterministic fake adapter exercises startup, SyncTest, frame advance, rollback, and failures | OS-independent CTest target |
| Platform smoke | Windows build success is the only adapter evidence | Shared conformance tests validate Windows clock/wait/config/files/network plus UDP loopback | Windows CTest target; mandatory for later adapters |
| Default build gate | Current build does not run tests | Native `cmake --build` runs self-contained CTest suites and propagates failure | Build log shows executed tests; intentional opt-out is explicit |

Performance changes, wire-format redesign, new public features, and a Linux or
Apple SDK binary are not acceptance criteria. The network protocol must remain
byte-compatible in this phase.

API and ABI compatibility are release-blocking. A mismatch is not accepted as a
portability tradeoff: the implementation must be redesigned additively or the
incompatible change moved to a separately versioned future proposal.

## Clarifications

- "Platform-agnostic ABI" means one versioned source contract with a stable ABI
  per target. It does not mean Windows DLLs are binary-loadable on Unix.
- Windows x64 is the compatibility gate because it is the current CI target.
  Win32/x86 may be measured in M0, but is not newly promised if the baseline is
  already unsupported.
- Existing [`port/multiplatform`](https://github.com/irregularthanis/ggpo-next/tree/port/multiplatform)
  and upstream [PR #81](https://github.com/pond3r/ggpo/pull/81) are reference
  implementations, not the baseline or a substitute for the boundary work.

