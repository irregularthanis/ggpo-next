# PRD — Portable Vector War and application E2E

Status: Draft

Source: inspected `5184cbf2d0e094c063d704e139631a6db94323bc`; native
cross-host infrastructure is tracked by [PR #5](https://github.com/irregularthanis/ggpo-next/pull/5)

Scope: interactive and headless Vector War on supported Windows and Linux
targets; excluded: Apple/mobile adapters, GGPO API/ABI or wire changes,
matchmaking, packaging Vector War as part of the SDK, and visual-performance
benchmarking

## Motivation

Vector War is GGPO's reference integration, but its Win32 process loop, GDI
renderer, keyboard input, timing, arena initialization, and performance monitor
prevent Linux developers from building or running it. Existing native tests
prove the library contract with a synthetic fixture; they do not prove that the
documented sample application remains a valid end-to-end consumer on every
supported platform.

The desired outcome is one maintainable application implementation that runs
interactively on Windows and Linux and supplies deterministic, automated
application-level interoperability evidence.

## Outcomes

- Build the interactive Vector War application on Windows and Linux from shared
  application/session sources and one SDL2 frontend.
- Preserve current gameplay, player and spectator modes, keyboard controls,
  connection/status presentation, checksums, and network diagnostics.
- Add a display-independent host using the same game state, callbacks, session
  setup, and frame path with scripted inputs and machine-readable results.
- Run native LL, WW, LW, and WL Vector War sessions after PR #5's fixture matrix;
  require deterministic state, actual rollback, expected events, and clean exit.
- Keep the public GGPO header, exports, ABI, and production UDP protocol
  unchanged. Missing SDL2 or unavailable native evidence must fail the relevant
  build/qualification rather than silently skip it.

## Success and constraints

| Metric | Baseline | Target | Validation |
| --- | --- | --- | --- |
| Supported interactive hosts | Windows/Win32 only | Windows and Linux | Native build plus one-frame SDL lifecycle smoke on both |
| Application correctness | Manual Windows sample | Shared deterministic baseline preserved | Scripted-input golden trace and headless local loopback |
| Cross-platform application sessions | None | LL, WW, LW, WL | Complete native E2E manifest from one source SHA |
| Rollback path | Not required by sample CI | Save/load/replay observed | Nonzero callback counters in every E2E scenario |
| Feature retention | Windows UI and scripts | Equivalent gameplay and diagnostics | Feature checklist plus focused host/input/render tests |
| Compatibility | Current public SDK | No API, ABI, export, or wire change | Existing Windows/Linux compatibility suites remain green |

Correctness gates use headless execution; rendering smoke proves frontend
lifecycle only. Pixel equality, frame rate, and input latency are not acceptance
claims.

## Clarifications

- “All supported platforms” currently means Windows and Linux, as documented in
  [Build](../../architecture/Build.md#platform-status). Future supported targets
  must qualify the same host contracts before joining the application matrix.
- SDL2 is selected because Ubuntu 24.04 packages it, Windows package managers
  provide both x64 and x86 builds, and upstream
  [PR #57](https://github.com/pond3r/ggpo/pull/57) demonstrates the approach.
  Reused code must retain its attribution; unrelated historical core changes are
  out of scope.
- PR #5 remains the source of truth for authenticated native orchestration. This
  work extends it with another fixture after it merges; it does not duplicate
  that control plane.
