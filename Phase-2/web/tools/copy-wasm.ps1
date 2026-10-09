# Copies the WebAssembly build of TeamForge into web/static/app so the server can serve it.
# Usage (from the repository root):  powershell -ExecutionPolicy Bypass -File web\tools\copy-wasm.ps1 -BuildDir build-wasm
param([string]$BuildDir = "build-wasm")
$ErrorActionPreference = "Stop"
$dest = Join-Path (Split-Path -Parent $PSScriptRoot) "static\app"
foreach ($name in @("TeamForge.js", "TeamForge.wasm", "qtloader.js")) {
    $source = Join-Path $BuildDir $name
    if (-not (Test-Path $source)) { throw "Missing $source - did the WebAssembly build finish?" }
    Copy-Item $source $dest -Force
    $size = [math]::Round((Get-Item $source).Length / 1MB, 1)
    Write-Host "copied $name ($size MB)"
}
Write-Host "Done. Start the server with:  python web\server.py"
