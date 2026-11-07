# PRD — Shared-core testing and CI consolidation

Status: Draft

Source: `d427210dc4b68f30dc6747de1a912dd6af4a3397`

Scope: reusable core, API, platform, native and CI test architecture; excluded:
production API/ABI/wire changes, new platform adapters, and Tailscale E2E
implementation.

## Motivation

GGPO Next currently proves native Windows/Linux builds, rollback smoke,
loopback UDP, package consumption, and frozen Windows compatibility. Portable
algorithms and most public API behavior remain covered only indirectly, while
the regular matrix repeats the same small test set across twelve SDK variants.

The goal is stronger deterministic correctness coverage, a reusable adapter
qualification contract, and measurably leaner regular CI. Coverage must be
established before any existing compatibility lane is removed. This makes a
third native platform primarily an adapter-and-qualification task rather than
a duplicated test-suite design.

## Outcomes

- Inventory all 16 exported `ggpo_*` functions, portable components, platform
  operations, existing tests, and compatibility gates.
- Add deterministic core and public API suites with independent expected state,
  including negative and lifecycle cases.
- Run one native conformance contract against Windows, Linux, and future
  adapters without copying assertions per OS.
- Preserve native loopback, shared/static consumers, Windows ABI/layout,
  Linux ABI/package audits, and full release qualification.
- Reduce regular PR runner cost only after replacement coverage is demonstrated;
  unavailable evidence leaves consolidation incomplete.

## Success and constraints

| Metric | Baseline | Target | Validation |
|---|---|---|---|
| Exported API classification | 16 exports; no coverage matrix | 16/16 classified as covered, partial, missing, or unspecified | Versioned inventory plus test references |
| Portable core coverage | SyncTest smoke; no focused queue/time/container suites | InputQueue, Sync, TimeSync, ring/static buffer and bit-vector invariants automated | Deterministic CTest with independent oracles |
| Adapter qualification | Fake contract plus separate native loopbacks | One shared conformance source passes Windows and Linux | Native CTest evidence per adapter |
| Regular CI cost | 4 Linux SDK lanes; 8 Windows variants plus final/header jobs | Lower measured runner minutes and/or critical path without lost gates | Before/after Actions evidence |
| Compatibility | Frozen Windows ABI/wire and installed consumers | No unintended API, ABI, calling-convention, package, or wire change | Existing baselines and cross-revision checks pass |

## Constraints

- Keep `GGPO::GGPO`, `BUILD_SHARED_LIBS`, C11/C++98 consumers, and supported
  package shapes.
- Tests may expose undefined behavior but must not freeze it without an explicit
  compatibility decision.
- Real native integration cannot be replaced by the fake adapter.
- PR #5 owns ephemeral cross-host networking; this work provides reusable
  artifacts and result contracts without implementing Tailscale.
