param(
    [switch]$Pristine,
    [string]$Board = "mini-keyboard/nrf52840",
    [string]$BuildDir = "build",
    [string]$Snippet = "rtt-console"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "ncs-env.ps1")

$NcsPython = "E:\ncs\toolchains\fd21892d0f\opt\bin\python.exe"
if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $ResolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir)
} else {
    $ResolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $BuildDir))
}

$westArgs = @(
    "build",
    "--build-dir", $ResolvedBuildDir,
    $ProjectRoot,
    "--board", $Board
)

if ($Pristine) {
    $westArgs += "--pristine=always"
}

$westArgs += "--"

if ($Snippet) {
    $westArgs += "-DSNIPPET=$Snippet"
}

$westArgs += "-DBOARD_ROOT=$ProjectRoot"

Write-Host "Running $NcsPython -m west $($westArgs -join ' ')"
& $NcsPython -m west @westArgs
if ($LASTEXITCODE -ne 0) {
    throw "west build failed with exit code $LASTEXITCODE"
}
