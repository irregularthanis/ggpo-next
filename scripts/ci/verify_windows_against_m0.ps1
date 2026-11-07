param(
    [Parameter(Mandatory = $true)][string]$M0Source,
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildRoot,
    [string]$EvidenceRoot
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$m0 = (Resolve-Path $M0Source).Path
if (!$BuildRoot) { $BuildRoot = Join-Path $repo "build/windows-final-qualification" }
if (!$EvidenceRoot) { $EvidenceRoot = Join-Path $repo "build/windows-final-evidence" }
$m0Build = Join-Path $BuildRoot "m0"
$candidateBuild = Join-Path $BuildRoot "candidate"
$m0Evidence = Join-Path $EvidenceRoot "m0"
$candidateEvidence = Join-Path $EvidenceRoot "candidate"

& (Join-Path $m0 "scripts/ci/windows_m0.ps1") `
    -Architecture x64 -Shared ON -Configuration Release `
    -Generator $Generator -BuildDirectory $m0Build `
    -EvidenceDirectory $m0Evidence
if ($LASTEXITCODE -ne 0) { throw "Frozen M0 qualification failed" }

& (Join-Path $PSScriptRoot "windows_compatibility_baseline.ps1") `
    -Architecture x64 -Shared ON -Configuration Release `
    -Generator $Generator -BuildDirectory $candidateBuild `
    -EvidenceDirectory $candidateEvidence
if ($LASTEXITCODE -ne 0) { throw "Candidate qualification failed" }

foreach ($artifact in @("exports.txt", "public-layout.txt", "packet-layout.txt")) {
    $difference = Compare-Object `
        (Get-Content (Join-Path $m0Evidence $artifact)) `
        (Get-Content (Join-Path $candidateEvidence $artifact))
    if ($difference) {
        $difference | Format-Table | Out-String | Write-Error
        throw "M0 and candidate $artifact evidence differs"
    }
}

$crossRoot = New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceRoot "cross-revision")
$m0Bin = Join-Path $m0Build "bin/x64/Release"
$candidateBin = Join-Path $candidateBuild "bin/x64/Release"
$runner = Join-Path $repo "tests/smoke/run_peer_smoke.py"
$crossCases = @(
    @{ Name = "m0-exe-candidate-dll"; Exe = (Join-Path $m0Bin "ggpo_m0_peer_smoke.exe"); Dll = $candidateBin },
    @{ Name = "candidate-exe-m0-dll"; Exe = (Join-Path $candidateBin "ggpo_windows_peer_smoke.exe"); Dll = $m0Bin }
)
$checksums = @()
foreach ($case in $crossCases) {
    $directory = New-Item -ItemType Directory -Force -Path (Join-Path $crossRoot $case.Name)
    Copy-Item $case.Exe (Join-Path $directory "ggpo_peer_smoke.exe")
    Copy-Item (Join-Path $case.Dll "GGPO.dll") $directory
    $executable = Join-Path $directory "ggpo_peer_smoke.exe"
    $summary = Join-Path $directory "peer-summary.json"
    & python $runner $executable --frames 180 --summary $summary
    if ($LASTEXITCODE -ne 0) { throw "$($case.Name) loopback failed" }
    $checksums += (Get-Content $summary | ConvertFrom-Json).runs[0].checksum
}
if ($checksums[0] -ne $checksums[1]) {
    throw "Cross-revision peer checksums differ: $($checksums -join ', ')"
}

[PSCustomObject]@{
    schema = 1
    m0_source = (git -C $m0 rev-parse HEAD)
    candidate_source = (git -C $repo rev-parse HEAD)
    compared = @("exports", "public layout", "packet layout")
    cross_revision = @("M0 executable with candidate DLL", "candidate executable with M0 DLL")
    frames = 180
    checksum = $checksums[0]
} | ConvertTo-Json | Set-Content -Encoding ascii `
    (Join-Path $EvidenceRoot "final-comparison.json")

Write-Host "M0 and candidate contracts and cross-revision runtime behavior match."
