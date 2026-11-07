# Windows compatibility baseline

This directory freezes the public and wire-layout expectations used before the
platform-boundary refactor. The executable fixtures fail on unexpected layout or
enum drift; shared builds also compare normalized DLL exports with the matching
`exports-x64.txt` or `exports-x86.txt` manifest.

## Test inventory

| Test | What it does | Detects | Does not establish |
| --- | --- | --- | --- |
| `windows_public_layout` | Compares public type sizes, field/callback offsets, and selected enum values with the frozen Windows ABI | Source-compatible changes that silently alter consumer memory layout or constants | Every possible source/API incompatibility |
| `windows_packet_layout` | Checks packed `UdpMsg` structures and representative payload/packet sizes | Wire-format drift caused by packing, field, or size changes | Network delivery or protocol behavior |
| `windows_synctest_smoke` | Feeds fixed inputs into a two-player SyncTest session and compares the final state with direct simulation | Broken save/load callbacks, rollback replay, input synchronization, or frame advancement | Real sockets or cross-process timing |
| `windows_peer_loopback` | Starts two processes on ephemeral localhost UDP ports and requires equal frame/checksum results | Windows socket, handshake, event, prediction, rollback, and cleanup failures | Internet, NAT, packet-loss, or cross-platform interoperability |
| DLL export comparison | Normalizes `dumpbin /exports` and compares x64/Win32 symbols with the frozen manifests | Missing, added, or renamed public DLL exports | Calling-convention/layout compatibility by itself |
| Installed consumers | Installs the SDK, then compiles and links C11 and C++98 programs using only that prefix | Non-C syntax, import/export errors, or missing consumer files | Runtime session behavior |

The shared [`smoke_session.cpp`](../../smoke/smoke_session.cpp) supplies the
small deterministic state machine and GGPO callbacks used by both runtime tests.
It is test state, not a model of a real game's simulation.

Run the complete legacy recipe from a Visual Studio developer PowerShell:

```powershell
./scripts/ci/windows_compatibility_baseline.ps1 `
  -Architecture x64 `
  -Shared ON `
  -Configuration Release `
  -Generator "Visual Studio 16 2019" `
  -NoiseRuns 10
```

Evidence contains the CMake cache, public and packet layout reports, normalized
exports, artifact SHA-256 values, environment identity, and repeated loopback
peer timings. Hashes identify artifacts; they are not an equality contract.

`-NoiseRuns` repeats the loopback scenario to characterize same-build timing
variation. It is evidence for designing later A/B comparisons, not a performance
benchmark or pass threshold.

The authoritative compatibility contract is the export/layout report plus the
deterministic SyncTest and peer results. The compatibility matrix qualifies both x64 and
Win32 in shared/static Debug/Release configurations.
