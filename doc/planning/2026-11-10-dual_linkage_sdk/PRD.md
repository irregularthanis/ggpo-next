# PRD — Dual-linkage SDK and CI consolidation

Status: Draft

Source: `59d831be59b1a6ff5a35bde31f112cfe7a3837ca`

Scope: one SDK per platform/architecture that contains both shared and static
GGPO, consumer-side linkage selection, documentation for linking each flavor
into an application, and a smaller CI/release matrix without redundant Debug
lanes. Excluded: public API, ABI, wire-format, or platform-adapter changes; new
platforms; Windows static-CRT (`/MT`) or Debug SDK distributions.

## Motivation

Linkage is currently chosen when GGPO is *built*. Each `BUILD_SHARED_LIBS`
value produces a separate install tree, CMake export, package, and CI lane:

- v0.3.0 published ten SDK assets: Windows x64/x86 × shared/static zips and
  Linux shared/static × DEB/RPM/tar.gz. The Linux DEB/RPM flavors conflict with
  each other, so one machine cannot hold both.
- The Windows compatibility workflow runs eight lanes (x64/Win32 ×
  shared/static × Debug/Release), and the release workflow builds every
  linkage separately.
- GGPO's `ASSERT` is unconditional, so Debug lanes only add the MSVC debug
  runtime checks. Repeating them across four library variants that compile the
  same sources adds runner time without finding anything new.

Applications should choose linkage when they *consume* GGPO, the way most
CMake SDKs work, and CI should test each distinct way of using it once.

## Outcomes

- One installed SDK per platform/architecture ships both `GGPO::shared` and
  `GGPO::static`. `GGPO::GGPO` stays the default (shared, or the only
  installed flavor), and `GGPO_USE_STATIC=ON` selects static.
- Existing consumers using `find_package(GGPO)` + `GGPO::GGPO` keep building
  without changes.
- SDK release assets drop from 10 to 5: `windows-x64.zip`, `windows-x86.zip`,
  and one Linux x86-64 DEB, RPM, and tar.gz.
- The Developer Guide shows, for Windows and Linux, how to link an application
  dynamically and statically, how to deploy the runtime for each, and how to
  build without CMake.
- Regular CI drops Debug from every lane but one, and tests both linkages from
  one build.

## Success and constraints

| Metric | Baseline (v0.3.0) | Target | Validation |
|---|---|---|---|
| SDK release assets | 10 + `SHA256SUMS` | 5 + `SHA256SUMS` | Release-candidate asset list |
| Release SDK build jobs | 2 Linux + 4 Windows | 1 Linux + 2 Windows | Workflow job graph |
| Windows compatibility lanes | 8 (4 Debug) | ≤ 5 (1 Debug) | Workflow job graph and Actions minutes |
| Linux compatibility lanes | 4 (gcc/clang × shared/static) | 2 (gcc/clang, both linkages each) | Workflow job graph |
| Consumer coverage | C11/C++98 against the single installed flavor | C11/C++98 × default/shared/static against one installed SDK, on both OSes | Installed-consumer CTest |
| Compatibility | Frozen ABI, exports, wire layout | Unchanged | Existing layout, export, and frozen-baseline gates |

## Constraints

- `ggponet.h`, the 16 exports, `GGPO_CALL`, struct layout, and the wire
  format are frozen. `GGPO_SHARED_LIB` must still reach only Windows consumers
  of the DLL (it selects `dllimport`).
- Windows shared and static libraries need distinct names: `GGPO.lib` (DLL
  import library) and `libGGPO.lib`. Linux keeps `libGGPO.so.0` and
  `libGGPO.a`.
- Windows SDKs stay Release with the dynamic CRT (`/MD`). PDBs are kept.
- Coverage is removed only when an equal or stronger gate replaces it: one
  Debug lane stays, and Windows memory checking moves to a scheduled MSVC
  AddressSanitizer job.
- Package and asset renames are a breaking distribution change, so this ships
  in 0.4.0 with migration notes.

## Users and stories

- **Application developer:** "I install one GGPO SDK and choose shared or
  static per target, without reinstalling or rebuilding GGPO."
- **Distribution packager:** "I ship a runtime package and a development
  package that install side by side and follow distro conventions."
- **Maintainer:** "PR CI tests each way GGPO can be used once, and releases
  build each platform/architecture once."

## Risks

| Risk | Mitigation |
|---|---|
| Existing scripts depend on per-flavor package names | 0.4.0 migration table; `Provides`/`Replaces` on the old Linux package names |
| A Windows consumer links the DLL without `dllimport` | `GGPO::shared` carries `GGPO_SHARED_LIB` as an interface definition; consumer test fails without it |
| Both flavors are linked into one binary | Config refuses `GGPO_USE_STATIC` with an invalid value and documents one flavor per target |
| Debug-only regressions are missed | Keep one x64 Debug lane (both libraries); add scheduled MSVC ASan |
