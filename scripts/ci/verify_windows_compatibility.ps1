param(
    [Parameter(Mandatory = $true)][string]$BaselineSource,
    [string]$Generator = "Visual Studio 17 2022",
    [string]$BuildRoot,
    [string]$EvidenceRoot
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$baseline = (Resolve-Path $BaselineSource).Path
if (!$BuildRoot) { $BuildRoot = Join-Path $repo "build/windows-final-qualification" }
if (!$EvidenceRoot) { $EvidenceRoot = Join-Path $repo "build/windows-final-evidence" }
$baselineBuild = Join-Path $BuildRoot "baseline"
$candidateBuild = Join-Path $BuildRoot "candidate"
$baselineEvidence = Join-Path $EvidenceRoot "baseline"
$candidateEvidence = Join-Path $EvidenceRoot "candidate"

# The immutable baseline checkout retains its original script and target names.
# Discover them by role so those historical names do not become part of the
# current compatibility interface.
$baselineRunners = @(
    Get-ChildItem (Join-Path $baseline "scripts/ci") -Filter "windows_*.ps1" |
        Where-Object { $_.Name -notmatch "^capture_" }
)
if ($baselineRunners.Count -ne 1) {
    throw "Expected one Windows qualification runner in the frozen baseline; found $($baselineRunners.Count)"
}
& $baselineRunners[0].FullName `
    -Architecture x64 -Shared ON -Configuration Release `
    -Generator $Generator -BuildDirectory $baselineBuild `
    -EvidenceDirectory $baselineEvidence
if ($LASTEXITCODE -ne 0) { throw "Frozen Windows baseline qualification failed" }

& (Join-Path $PSScriptRoot "windows_compatibility_baseline.ps1") `
    -Architecture x64 -Shared ON -Configuration Release `
    -Generator $Generator -BuildDirectory $candidateBuild `
    -EvidenceDirectory $candidateEvidence
if ($LASTEXITCODE -ne 0) { throw "Candidate qualification failed" }

foreach ($artifact in @("exports.txt", "public-layout.txt", "packet-layout.txt")) {
    $difference = Compare-Object `
        (Get-Content (Join-Path $baselineEvidence $artifact)) `
        (Get-Content (Join-Path $candidateEvidence $artifact))
    if ($difference) {
        $difference | Format-Table | Out-String | Write-Error
        throw "Windows baseline and candidate $artifact evidence differs"
    }
}

$crossRoot = New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceRoot "cross-revision")
$baselineBin = Join-Path $baselineBuild "bin/x64/Release"
$candidateBin = Join-Path $candidateBuild "bin/x64/Release"
$runner = Join-Path $repo "tests/smoke/run_peer_smoke.py"
$baselinePeerExecutables = @(Get-ChildItem $baselineBin -Filter "ggpo_*_peer_smoke.exe")
if ($baselinePeerExecutables.Count -ne 1) {
    throw "Expected one peer-smoke executable in the frozen baseline; found $($baselinePeerExecutables.Count)"
}
$crossCases = @(
    @{ Name = "baseline-exe-candidate-dll"; Exe = $baselinePeerExecutables[0].FullName; Dll = $candidateBin },
    @{ Name = "candidate-exe-baseline-dll"; Exe = (Join-Path $candidateBin "ggpo_windows_peer_smoke.exe"); Dll = $baselineBin }
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
    baseline_source = (git -C $baseline rev-parse HEAD)
    candidate_source = (git -C $repo rev-parse HEAD)
    compared = @("exports", "public layout", "packet layout")
    cross_revision = @("baseline executable with candidate DLL", "candidate executable with baseline DLL")
    frames = 180
    checksum = $checksums[0]
} | ConvertTo-Json | Set-Content -Encoding ascii `
    (Join-Path $EvidenceRoot "final-comparison.json")

Write-Host "Windows baseline and candidate contracts and cross-revision runtime behavior match."
