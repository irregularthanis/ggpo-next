param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][ValidateSet("x64", "Win32")][string]$Architecture,
    [Parameter(Mandatory = $true)][ValidateSet("ON", "OFF")][string]$Shared,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [int]$NoiseRuns = 0
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$build = (Resolve-Path $BuildDirectory).Path
$output = New-Item -ItemType Directory -Force -Path $OutputDirectory
$prefix = if ($Architecture -eq "x64") { "x64" } else { "x86" }
$binaryDirectory = Join-Path $build "bin/$prefix/$Configuration"
$libraryDirectory = Join-Path $build "lib/$prefix/$Configuration"

function Invoke-CapturedExecutable {
    param([string]$Executable, [string]$Destination)
    & $Executable 2>&1 | Tee-Object -FilePath $Destination
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

function Find-DumpBin {
    $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio/Installer/vswhere.exe"
    if (!(Test-Path $vswhere)) {
        throw "dumpbin.exe is not on PATH and vswhere.exe was not found"
    }

    $target = if ($Architecture -eq "x64") { "x64" } else { "x86" }
    $matches = @(
        & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find "VC\Tools\MSVC\**\bin\Hostx64\$target\dumpbin.exe"
    )
    $dumpbin = $matches | Select-Object -Last 1
    if (!$dumpbin -or !(Test-Path $dumpbin)) {
        throw "Unable to locate the MSVC $target dumpbin.exe"
    }
    return $dumpbin
}

Invoke-CapturedExecutable `
    (Join-Path $binaryDirectory "ggpo_m0_public_layout.exe") `
    (Join-Path $output "public-layout.txt")
Invoke-CapturedExecutable `
    (Join-Path $binaryDirectory "ggpo_m0_packet_layout.exe") `
    (Join-Path $output "packet-layout.txt")

if ($Shared -eq "ON") {
    $dll = Join-Path $binaryDirectory "GGPO.dll"
    $dumpbin = Find-DumpBin
    $exports = & $dumpbin /nologo /exports $dll
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed with exit code $LASTEXITCODE"
    }
    Set-Content -Encoding ascii `
        (Join-Path $output "dumpbin-exports.txt") `
        -Value $exports
    $symbols = @(
        $exports | ForEach-Object {
            if ($_ -match '\b(ggpo_[A-Za-z0-9_]+)\b') {
                $Matches[1]
            }
        } | Sort-Object -Unique
    )
    Set-Content -Encoding ascii (Join-Path $output "exports.txt") -Value $symbols

    if ($Architecture -eq "x64") {
        $expected = Get-Content (Join-Path $repo "tests/baseline/windows/exports-x64.txt")
        $difference = Compare-Object $expected $symbols
        if ($difference) {
            $difference | Format-Table | Out-String | Write-Error
            throw "x64 DLL exports differ from the M0 baseline"
        }
    }
}

Copy-Item (Join-Path $build "CMakeCache.txt") (Join-Path $output "CMakeCache.txt")

$artifactDirectories = @($binaryDirectory, $libraryDirectory) |
    Where-Object { Test-Path $_ }
$artifacts = Get-ChildItem $artifactDirectories -File |
    Where-Object { $_.Extension -in ".dll", ".exe", ".lib", ".pdb" } |
    ForEach-Object {
        $hash = Get-FileHash -Algorithm SHA256 $_.FullName
        [PSCustomObject]@{
            file = $_.FullName.Substring($build.Length + 1).Replace("\", "/")
            bytes = $_.Length
            sha256 = $hash.Hash.ToLowerInvariant()
        }
    }
$artifacts | ConvertTo-Json | Set-Content -Encoding ascii (Join-Path $output "artifacts.json")

if ($NoiseRuns -gt 0) {
    $peer = Join-Path $binaryDirectory "ggpo_m0_peer_smoke.exe"
    & python (Join-Path $repo "tests/smoke/run_peer_smoke.py") `
        $peer --frames 180 --runs $NoiseRuns `
        --summary (Join-Path $output "peer-noise.json")
    if ($LASTEXITCODE -ne 0) {
        throw "peer noise capture failed with exit code $LASTEXITCODE"
    }
}

[PSCustomObject]@{
    schema = 1
    source = (git -C $repo rev-parse HEAD)
    architecture = $Architecture
    configuration = $Configuration
    shared = $Shared
    cmake = (cmake --version | Select-Object -First 1)
} | ConvertTo-Json | Set-Content -Encoding ascii (Join-Path $output "environment.json")
