param(
    [switch]$Pristine,
    [string]$Board = "mini-keyboard/nrf52840",
    [string]$BuildDir = "build",
    [string]$Snippet = "rtt-console"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "ncs-env.ps1")

$West = Join-Path "E:\ncs\toolchains\fd21892d0f\opt\bin\Scripts" "west.exe"
$ResolvedBuildDir = Join-Path $ProjectRoot $BuildDir

$westArgs = @(
    "build",
    "--build-dir", $ResolvedBuildDir,
    $ProjectRoot,
    "--board", $Board,
    "--"
)

if ($Pristine) {
    $westArgs = @("build", "--build-dir", $ResolvedBuildDir, $ProjectRoot, "--pristine", "--board", $Board, "--")
}

if ($Snippet) {
    $westArgs += "-DSNIPPET=$Snippet"
}

$westArgs += "-DBOARD_ROOT=$ProjectRoot"

Write-Host "Running west $($westArgs -join ' ')"
& $West @westArgs