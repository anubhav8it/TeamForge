# Downloads the two libraries TeamForge needs into this folder.
# Run from PowerShell:   cd third_party ; .\get-deps.ps1
# If PowerShell blocks the script:  powershell -ExecutionPolicy Bypass -File .\get-deps.ps1
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$httplib = "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.15.3/httplib.h"
$sqlite  = "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"

Write-Host "Downloading cpp-httplib..."
Invoke-WebRequest -Uri $httplib -OutFile "httplib.h"

Write-Host "Downloading SQLite..."
Invoke-WebRequest -Uri $sqlite -OutFile "sqlite.zip"
Expand-Archive -Path "sqlite.zip" -DestinationPath "sqlite-tmp" -Force
$dir = Get-ChildItem "sqlite-tmp" -Directory | Select-Object -First 1
Copy-Item "$($dir.FullName)\sqlite3.c", "$($dir.FullName)\sqlite3.h" -Destination . -Force
Remove-Item "sqlite.zip", "sqlite-tmp" -Recurse -Force

Write-Host "Done: httplib.h, sqlite3.c, sqlite3.h"
