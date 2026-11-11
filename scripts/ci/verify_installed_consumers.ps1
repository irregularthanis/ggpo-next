param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][ValidateSet("x64", "Win32")][string]$Architecture,
    [Parameter(Mandatory = $true)][ValidateSet("ON", "OFF")][string]$Shared,
    [Parameter(Mandatory = $true)][string]$Generator,
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = (Resolve-Path $BuildDirectory).Path
$install = Join-Path $build "installed-consumer-prefix"
$consumerBuild = Join-Path $build "installed-consumer-build"

cmake --install $build --config $Configuration --prefix $install
if ($LASTEXITCODE -ne 0) { throw "GGPO install failed" }
$evidence = New-Item -ItemType Directory -Force -Path $EvidenceDirectory

# One driver for every platform: builds default, GGPO_USE_STATIC=ON and
# GGPO::shared consumers for whichever flavors are installed, then runs them.
# Static consumers run with the SDK bin directory removed from PATH, which
# proves they do not load GGPO.dll.
python (Join-Path $repo "tests/consumer/verify_consumers.py") `
    --prefix $install `
    --work $consumerBuild `
    --config $Configuration `
    --generator $Generator `
    --architecture $Architecture `
    --report (Join-Path $evidence "installed-consumer-cases.json")
if ($LASTEXITCODE -ne 0) { throw "Installed-consumer verification failed" }

[PSCustomObject]@{
    schema = 1
    source = (git -C $repo rev-parse HEAD)
    architecture = $Architecture
    configuration = $Configuration
    shared = $Shared
    c_consumer = "C11 compiled, linked and run"
    cpp_consumer = "C++98 compiled, linked and run"
    source_tree_includes = $false
} | ConvertTo-Json | Set-Content -Encoding ascii `
    (Join-Path $evidence "installed-consumers.json")

Write-Host "Installed C11 and C++98 consumers built and ran for every installed linkage."
