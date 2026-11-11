# PRD — MinGW-w64 SDK support

Status: Draft

Source: `02ac787240fe68dfe21ce60629fe6980f9885e3b` (stacked on [PR #9](https://github.com/irregularthanis/ggpo-next/pull/9))

Scope: build, test, package, and document the GGPO SDK with MinGW-w64 for
Windows x64 and x86, shipping both the shared and the static library.
Excluded: a MinGW Vector War build or download, Clang/LLVM-MinGW toolchains,
ARM64, and any public API, ABI, export, or wire-format change.

## Motivation

GGPO supports Windows only through MSVC. Games and engines built with
MinGW-w64 (MSYS2, cross-compiling from Linux, many open-source toolchains)
cannot link the MSVC static library at all, and have no supported or tested
path to the DLL. The core is already portable: the only Windows-specific
source is `platform_windows.cpp`, and the public API is C with `__cdecl`.
The gaps are in the build system, which treats `WIN32` as "MSVC", and in CI
and packaging.

## Outcomes

- `cmake` with MinGW-w64 builds both libraries for x64 and x86, using MinGW
  naming: `libGGPO.dll` + `libGGPO.dll.a` (shared) and `libGGPO.a` (static).
- The same CMake package works unchanged: `GGPO::GGPO`, `GGPO::shared`,
  `GGPO::static`, and `GGPO_USE_STATIC`.
- CI builds and runs the full CTest suite and the installed-consumer cases
  natively on Windows for both architectures.
- A MinGW-built peer plays a networked session against MSVC and Linux peers.
- Releases add `windows-x64-mingw.zip` and `windows-x86-mingw.zip`.
- The Developer Guide documents MinGW installation and linking, and which
  artifacts can and cannot be mixed between MSVC and MinGW.

## Success and constraints

| Metric | Baseline | Target | Validation |
|---|---|---|---|
| MinGW configure and build | Fails (MSVC flags, `.lib` names) | Both libraries build on x64 and x86 | CI job per architecture |
| Tests | None for MinGW | Full CTest suite passes | Same CI jobs |
| Installed consumers | MSVC and Linux only | Default, static and shared cases pass on both architectures | `verify_consumers.py` |
| Interoperability | Untested | MinGW↔MSVC and MinGW↔Linux sessions converge | E2E scenarios |
| Exports and layout | MSVC frozen baseline | Same 16 `ggpo_*` exports and same public layout values | Layout fixtures + `objdump -p` manifest |
| Release assets | 5 SDK assets | 7 (+2 MinGW zips) | Release dry run |
| MSVC and Linux | Current gates | Unchanged | Existing workflows and frozen baseline |

## Constraints

- No change to `ggponet.h` declarations, exports, struct layout, calling
  convention, or wire format. The frozen MSVC baseline stays authoritative.
- MSVC builds, names (`GGPO.dll`, `GGPO.lib`, `libGGPO.lib`) and packages
  must not change.
- Runtimes: x64 targets **UCRT** (MSYS2 `UCRT64`). x86 targets **MSVCRT**
  (MSYS2 `MINGW32`), because MSYS2 provides no 32-bit UCRT environment. Both
  are documented per download.
- MinGW static libraries are never mixed with MSVC applications, and vice
  versa. Only the DLL's C interface is expected to cross toolchains.
- Vector War is not built or shipped for MinGW.

## Users and stories

- **MinGW game developer:** "I download the MinGW SDK, call
  `find_package(GGPO)`, and choose shared or static like MSVC users do."
- **Cross-compiling developer:** "I build GGPO from source with a
  `x86_64-w64-mingw32` toolchain on Linux using the documented CMake command."
- **Maintainer:** "MinGW regressions fail PR CI, without slowing the MSVC and
  Linux lanes."

## Risks

| Risk | Mitigation |
|---|---|
| `WIN32` checks silently apply MSVC flags to MinGW | Audit every `WIN32`/`MSVC` branch; MinGW CI fails on unknown flags |
| Different C runtime per architecture confuses users | One runtime per download, named in the guide and the zip README |
| x86 exports get decorated names (`_ggpo_idle`, `@N`) | Export manifest check on both architectures |
| Users mix MinGW static libraries with MSVC apps | Separate downloads and an explicit compatibility table |
| CI cost grows | Two Release-only jobs, no Debug lane; E2E adds two scenarios only |
