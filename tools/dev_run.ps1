param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $Root "out\build"
$Cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Exe = Join-Path $BuildDir "$Config\ShaderViewer.exe"

function Stop-Viewer {
    cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
}

if (-not $NoBuild) {
    if (-not (Test-Path $BuildDir)) {
        & $Cmake -S $Root -B $BuildDir -G "Visual Studio 17 2022" -A x64
    }
    & $Cmake --build $BuildDir --config $Config --target ShaderViewer
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $Exe)) {
    Write-Error "Executable not found: $Exe"
}

Stop-Viewer
Start-Sleep -Milliseconds 200
Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe)
Write-Output "Running $Exe"
