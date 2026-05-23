$ErrorActionPreference = "Stop"

$NcsRoot = "E:\ncs\v3.2.3"
$ToolchainRoot = "E:\ncs\toolchains\fd21892d0f"
$ToolchainBin = Join-Path $ToolchainRoot "opt\bin"
$PythonScripts = Join-Path $ToolchainBin "Scripts"
$ZephyrSdk = Join-Path $ToolchainRoot "opt\zephyr-sdk"

if (-not (Test-Path $NcsRoot)) {
    throw "NCS root not found: $NcsRoot"
}

if (-not (Test-Path $ToolchainRoot)) {
    throw "NCS toolchain not found: $ToolchainRoot"
}

$env:ZEPHYR_BASE = Join-Path $NcsRoot "zephyr"
$env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
$env:ZEPHYR_SDK_INSTALL_DIR = $ZephyrSdk
$NcsPython = Join-Path $ToolchainBin "python.exe"
$env:WEST_PYTHON = $NcsPython
$env:NCS_TOOLCHAIN_VERSION = "NONE"

$pathEntries = @(
    $ToolchainBin,
    $PythonScripts,
    (Join-Path $ZephyrSdk "arm-zephyr-eabi\bin"),
    (Join-Path $ToolchainRoot "usr\bin"),
    (Join-Path $ToolchainRoot "bin")
)

$env:PATH = (($pathEntries + ($env:PATH -split ';')) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique) -join ';'

Write-Host "NCS root: $NcsRoot"
Write-Host "Toolchain: $ToolchainRoot"
Write-Host "ZEPHYR_BASE: $env:ZEPHYR_BASE"
Write-Host "Python: $NcsPython"
& $NcsPython -m west --version
if ($LASTEXITCODE -ne 0) {
    throw "west version check failed with exit code $LASTEXITCODE"
}
