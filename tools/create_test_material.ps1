$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$Root = Split-Path $PSScriptRoot -Parent
$OutDir = Join-Path $Root "assets\test_material"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$size = 512
$normal = New-Object Drawing.Bitmap $size, $size
$albedo = New-Object Drawing.Bitmap $size, $size

for ($y = 0; $y -lt $size; $y++) {
    for ($x = 0; $x -lt $size; $x++) {
        $fx = $x / [double]$size
        $fy = $y / [double]$size
        $nx = [Math]::Sin($fx * 18.0) * [Math]::Cos($fy * 11.0) * 0.45
        $ny = [Math]::Cos($fx * 13.0) * [Math]::Sin($fy * 17.0) * 0.45
        $r = [byte][Math]::Max(0, [Math]::Min(255, ($nx + 1.0) * 127.5))
        $g = [byte][Math]::Max(0, [Math]::Min(255, ($ny + 1.0) * 127.5))
        $normal.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, $r, $g, 255))
        $albedo.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, 180, 120, 90))
    }
}

$normalPath = Join-Path $OutDir "ProofMaterial_Normal.png"
$albedoPath = Join-Path $OutDir "ProofMaterial_BaseColor.png"
$roughPath = Join-Path $OutDir "ProofMaterial_Roughness.png"
$normal.Save($normalPath, [Drawing.Imaging.ImageFormat]::Png)
$albedo.Save($albedoPath, [Drawing.Imaging.ImageFormat]::Png)
(New-Object Drawing.Bitmap 4, 4).Save($roughPath, [Drawing.Imaging.ImageFormat]::Png)

Write-Output "Created test material in $OutDir"
