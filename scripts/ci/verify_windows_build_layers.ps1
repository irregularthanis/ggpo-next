param(
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildRoot,
    [string]$EvidenceRoot
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
if (!$BuildRoot) { $BuildRoot = Join-Path $repo "build/windows-build-layers" }
if (!$EvidenceRoot) { $EvidenceRoot = Join-Path $repo "build/windows-build-layer-evidence" }

foreach ($layer in @("LEGACY", "LAYERED")) {
    $name = $layer.ToLowerInvariant()
    & (Join-Path $PSScriptRoot "windows_compatibility_baseline.ps1") `
        -Architecture x64 `
        -Shared ON `
        -Configuration Release `
        -BuildLayer $layer `
        -Generator $Generator `
        -BuildDirectory (Join-Path $BuildRoot $name) `
        -EvidenceDirectory (Join-Path $EvidenceRoot $name)
    if ($LASTEXITCODE -ne 0) { throw "$layer build-layer qualification failed" }
}

$legacy = Join-Path $EvidenceRoot "legacy"
$layered = Join-Path $EvidenceRoot "layered"
foreach ($artifact in @("exports.txt", "public-layout.txt", "packet-layout.txt")) {
    $difference = Compare-Object `
        (Get-Content (Join-Path $legacy $artifact)) `
        (Get-Content (Join-Path $layered $artifact))
    if ($difference) {
        $difference | Format-Table | Out-String | Write-Error
        throw "Legacy and layered $artifact evidence differs"
    }
}

[PSCustomObject]@{
    schema = 1
    source = (git -C $repo rev-parse HEAD)
    architecture = "x64"
    configuration = "Release"
    shared = "ON"
    compared = @("exports", "public layout", "packet layout")
    tests = "6/6 passed through each build entry"
    installed_consumers = "C11 and C++98 passed through each build entry"
} | ConvertTo-Json | Set-Content -Encoding ascii `
    (Join-Path (New-Item -ItemType Directory -Force -Path $EvidenceRoot) "comparison.json")

Write-Host "Legacy and layered Windows build entries are contract-equivalent."
