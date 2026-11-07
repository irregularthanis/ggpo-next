param(
    [ValidateSet("x64", "Win32")][string]$Architecture = "x64",
    [ValidateSet("ON", "OFF")][string]$Shared = "ON",
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$BuildDirectory,
    [string]$EvidenceDirectory,
    [int]$NoiseRuns = 0
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$sharedLabel = if ($Shared -eq "ON") { "shared" } else { "static" }
$architectureLabel = if ($Architecture -eq "x64") { "x64" } else { "x86" }

if (!$BuildDirectory) {
    $BuildDirectory = Join-Path $repo "build/windows-compatibility-$architectureLabel-$sharedLabel"
}
if (!$EvidenceDirectory) {
    $EvidenceDirectory = Join-Path $repo "build/windows-compatibility-evidence/$architectureLabel-$sharedLabel-$Configuration"
}

cmake -S $repo -B $BuildDirectory -G $Generator -A $Architecture `
    -DBUILD_SHARED_LIBS=$Shared `
    -DGGPO_BUILD_SDK=ON `
    -DGGPO_BUILD_VECTORWAR=ON `
    -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed" }

& (Join-Path $PSScriptRoot "verify_installed_consumers.ps1") `
    -BuildDirectory $BuildDirectory `
    -Configuration $Configuration `
    -Architecture $Architecture `
    -Shared $Shared `
    -Generator $Generator `
    -EvidenceDirectory $EvidenceDirectory
if ($LASTEXITCODE -ne 0) { throw "Installed-consumer verification failed" }

& (Join-Path $PSScriptRoot "capture_windows_compatibility_baseline.ps1") `
    -BuildDirectory $BuildDirectory `
    -Configuration $Configuration `
    -Architecture $Architecture `
    -Shared $Shared `
    -OutputDirectory $EvidenceDirectory `
    -NoiseRuns $NoiseRuns
if ($LASTEXITCODE -ne 0) { throw "Windows compatibility evidence capture failed" }

Write-Host "Windows compatibility evidence: $EvidenceDirectory"
