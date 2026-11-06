# Windows compatibility baseline

This directory freezes the public and wire-layout expectations used before the
platform-boundary refactor. The executable fixtures fail on unexpected layout or
enum drift; shared x64 builds also compare the normalized DLL exports with
`exports-x64.txt`.

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

The authoritative compatibility contract is the export/layout report plus the
deterministic SyncTest and peer results. The compatibility matrix qualifies both x64 and
Win32 in shared/static Debug/Release configurations.
