param(
    [string]$PngPath = (Join-Path $PSScriptRoot "..\assets\app_icon.png"),
    [string]$IcoPath = (Join-Path $PSScriptRoot "..\resources\app.ico")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $PngPath)) {
    Write-Error "Icon PNG not found: $PngPath"
}

Add-Type -AssemblyName System.Drawing

$icoDir = Split-Path $IcoPath -Parent
if (-not (Test-Path $icoDir)) {
    New-Item -ItemType Directory -Force -Path $icoDir | Out-Null
}

$source = $null
$scaled = $null
$icon = $null
$stream = $null

try {
    $source = [System.Drawing.Bitmap]::FromFile((Resolve-Path $PngPath))
    $size = 256
    $scaled = New-Object System.Drawing.Bitmap $size, $size
    $graphics = [System.Drawing.Graphics]::FromImage($scaled)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.DrawImage($source, 0, 0, $size, $size)
    $graphics.Dispose()

    $iconHandle = $scaled.GetHicon()
    $icon = [System.Drawing.Icon]::FromHandle($iconHandle)
    $stream = [System.IO.File]::Create($IcoPath)
    $icon.Save($stream)
    Write-Output "Wrote $IcoPath"
} finally {
    if ($stream) { $stream.Dispose() }
    if ($icon) { $icon.Dispose() }
    if ($scaled) { $scaled.Dispose() }
    if ($source) { $source.Dispose() }
}
