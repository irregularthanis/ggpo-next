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
$installForCMake = $install.Replace("\", "/")

cmake -S (Join-Path $repo "tests/consumer") -B $consumerBuild `
    -G $Generator -A $Architecture `
    "-DGGPO_INSTALL_PREFIX:PATH=$installForCMake" `
    -DGGPO_SHARED_LIB=$Shared
if ($LASTEXITCODE -ne 0) { throw "Installed-consumer configure failed" }

cmake --build $consumerBuild --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Installed-consumer compile/link failed" }

$evidence = New-Item -ItemType Directory -Force -Path $EvidenceDirectory
[PSCustomObject]@{
    schema = 1
    source = (git -C $repo rev-parse HEAD)
    architecture = $Architecture
    configuration = $Configuration
    shared = $Shared
    c_consumer = "C11 compiled and linked"
    cpp_consumer = "C++98 compiled and linked"
    source_tree_includes = $false
} | ConvertTo-Json | Set-Content -Encoding ascii `
    (Join-Path $evidence "installed-consumers.json")

Write-Host "Installed C11 and C++98 consumers compiled and linked successfully."
