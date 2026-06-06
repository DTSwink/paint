param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $Root "out\build"
$Cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Exe = Join-Path $BuildDir "$Config\ShaderViewer.exe"
$InstallDir = Join-Path $env:LOCALAPPDATA "ShaderViewer"
$DesktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Shader Viewer.lnk"

function Stop-Viewer {
    cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
}

Write-Output "Generating application icon..."
powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "generate_icon.ps1")

if (-not $SkipBuild) {
    if (-not (Test-Path $BuildDir)) {
        & $Cmake -S $Root -B $BuildDir -G "Visual Studio 17 2022" -A x64
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    Stop-Viewer
    & $Cmake --build $BuildDir --config $Config --target ShaderViewer
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $Exe)) {
    Write-Error "Executable not found: $Exe"
}

Write-Output "Installing to $InstallDir ..."

$SettingsBackup = $null
if (Test-Path $InstallDir) {
    $existingSettings = Join-Path $InstallDir "settings"
    if (Test-Path $existingSettings) {
        $SettingsBackup = Join-Path $env:TEMP "ShaderViewer-settings-backup"
        if (Test-Path $SettingsBackup) {
            Remove-Item -Recurse -Force $SettingsBackup
        }
        Copy-Item -Recurse $existingSettings $SettingsBackup
    }
    Remove-Item -Recurse -Force $InstallDir
}
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

Copy-Item $Exe (Join-Path $InstallDir "ShaderViewer.exe")
Copy-Item -Recurse (Join-Path $Root "shaders") (Join-Path $InstallDir "shaders")
Copy-Item -Recurse (Join-Path $Root "assets") (Join-Path $InstallDir "assets")

if ($SettingsBackup -and (Test-Path $SettingsBackup)) {
    Copy-Item -Recurse $SettingsBackup (Join-Path $InstallDir "settings")
} elseif (Test-Path (Join-Path $Root "settings")) {
    Copy-Item -Recurse (Join-Path $Root "settings") (Join-Path $InstallDir "settings")
} else {
    New-Item -ItemType Directory -Force -Path (Join-Path $InstallDir "settings") | Out-Null
}

Copy-Item (Join-Path $Root "resources\app.ico") (Join-Path $InstallDir "app.ico")

Write-Output "Creating desktop shortcut..."
& (Join-Path $PSScriptRoot "update_desktop_shortcut.ps1") `
    -TargetExe (Join-Path $InstallDir "ShaderViewer.exe") `
    -WorkingDirectory $InstallDir `
    -IconPath (Join-Path $InstallDir "app.ico")

Write-Output ""
Write-Output "Shader Viewer installed."
Write-Output "  App:      $(Join-Path $InstallDir 'ShaderViewer.exe')"
Write-Output "  Shortcut: $DesktopShortcut"
Write-Output ""
Write-Output "Double-click 'Shader Viewer' on your desktop to launch."
