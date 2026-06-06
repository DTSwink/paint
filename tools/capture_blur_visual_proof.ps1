$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Get-Channel {
    param([int]$Px, [int]$Shift)
    return ($Px -shr $Shift) -band 255
}

function Get-SobelEdgeScore {
    param([Drawing.Bitmap]$Bmp, [int]$Step = 2)
    $w = $Bmp.Width
    $h = $Bmp.Height
    $rect = New-Object Drawing.Rectangle 0, 0, $w, $h
    $bits = $Bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $sum = 0.0
        $count = 0
        for ($y = 1; $y -lt ($h - 1); $y += $Step) {
            for ($x = 1; $x -lt ($w - 1); $x += $Step) {
                $read = {
                    param($ox, $oy)
                    $px = [Runtime.InteropServices.Marshal]::ReadInt32($bits.Scan0, ($y + $oy) * $bits.Stride + ($x + $ox) * 4)
                    $r = Get-Channel $px 16
                    $g = Get-Channel $px 8
                    $b = Get-Channel $px 0
                    return ($r + $g + $b) / 3.0
                }
                $tl = & $read -1 -1; $tc = & $read 0 -1; $tr = & $read 1 -1
                $ml = & $read -1 0;  $mr = & $read 1 0
                $bl = & $read -1 1; $bc = & $read 0 1;  $br = & $read 1 1
                $gx = -$tl - 2 * $ml - $bl + $tr + 2 * $mr + $br
                $gy = -$tl - 2 * $tc - $tr + $bl + 2 * $bc + $br
                $sum += [Math]::Sqrt($gx * $gx + $gy * $gy)
                $count++
            }
        }
        return @{ Score = $sum / [Math]::Max($count, 1); Samples = $count }
    } finally {
        $Bmp.UnlockBits($bits)
    }
}

function Get-FlatBlockRatio {
    param([Drawing.Bitmap]$Bmp, [int]$Block = 8, [double]$VarianceThreshold = 12.0)
    $w = $Bmp.Width
    $h = $Bmp.Height
    $rect = New-Object Drawing.Rectangle 0, 0, $w, $h
    $bits = $Bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $flatBlocks = 0
        $totalBlocks = 0
        for ($by = 0; $by + $Block -le $h; $by += $Block) {
            for ($bx = 0; $bx + $Block -le $w; $bx += $Block) {
                $vals = New-Object System.Collections.Generic.List[double]
                for ($y = 0; $y -lt $Block; $y++) {
                    for ($x = 0; $x -lt $Block; $x++) {
                        $px = [Runtime.InteropServices.Marshal]::ReadInt32($bits.Scan0, ($by + $y) * $bits.Stride + ($bx + $x) * 4)
                        $r = Get-Channel $px 16
                        $g = Get-Channel $px 8
                        $b = Get-Channel $px 0
                        $vals.Add(($r + $g + $b) / 3.0)
                    }
                }
                $mean = ($vals | Measure-Object -Average).Average
                $var = 0.0
                foreach ($v in $vals) { $var += ($v - $mean) * ($v - $mean) }
                $var /= $vals.Count
                if ($var -lt $VarianceThreshold) { $flatBlocks++ }
                $totalBlocks++
            }
        }
        return 100.0 * $flatBlocks / [Math]::Max($totalBlocks, 1)
    } finally {
        $Bmp.UnlockBits($bits)
    }
}

$Root = Split-Path $PSScriptRoot -Parent
$ProofDir = Join-Path $Root "out\proof"
New-Item -ItemType Directory -Force -Path $ProofDir | Out-Null

& (Join-Path $PSScriptRoot "create_test_material.ps1") | Out-Null

$Exe = Join-Path $Root "out\build\Release\ShaderViewer.exe"
$Cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $Cmake --build (Join-Path $Root "out\build") --config Release --target ShaderViewer
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$CapturePath = Join-Path $ProofDir "blur_preset_capture.png"
cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList "--proof-capture=$CapturePath" -PassThru -Wait
if ($p.ExitCode -ne 0) { throw "Capture failed with exit code $($p.ExitCode)" }
if (-not (Test-Path $CapturePath)) { throw "Missing capture: $CapturePath" }

$full = [Drawing.Bitmap]::FromFile($CapturePath)
$sidebar = 380
$vpW = $full.Width - $sidebar
$halfW = [int][Math]::Floor($vpW / 2)
$h = $full.Height

$orig = New-Object Drawing.Bitmap $halfW, $h
$mod = New-Object Drawing.Bitmap $halfW, $h
$og = [Drawing.Graphics]::FromImage($orig)
$mg = [Drawing.Graphics]::FromImage($mod)
$og.DrawImage($full, 0, 0, (New-Object Drawing.Rectangle $sidebar, 0, $halfW, $h), [Drawing.GraphicsUnit]::Pixel)
$mg.DrawImage($full, 0, 0, (New-Object Drawing.Rectangle ($sidebar + $halfW), 0, $halfW, $h), [Drawing.GraphicsUnit]::Pixel)
$og.Dispose(); $mg.Dispose(); $full.Dispose()

$origEdge = Get-SobelEdgeScore $orig
$modEdge = Get-SobelEdgeScore $mod
$flatPct = Get-FlatBlockRatio $mod

$reportPath = Join-Path $ProofDir "blur_visual_report.txt"
@"
Blur visual proof
=================
Capture: $CapturePath
Original edge score:  $($origEdge.Score.ToString('N2'))
Modified edge score:  $($modEdge.Score.ToString('N2'))
Modified flat blocks: $($flatPct.ToString('N1'))% (8x8 variance < 12)
"@ | Set-Content -Path $reportPath -Encoding UTF8

Write-Output (Get-Content $reportPath)

# Faceted mesh blur shows many flat blocks AND high edge score on modified side.
if ($flatPct -gt 35.0 -and $modEdge.Score -gt ($origEdge.Score * 0.85)) {
    Write-Error "Blur looks faceted (flat blocks=$flatPct%, edge=$($modEdge.Score))"
}

if ($modEdge.Score -ge $origEdge.Score) {
    Write-Error "Modified view is not smoother than original (edge $($modEdge.Score) vs $($origEdge.Score))"
}

$meanLuma = 0.0
$rect = New-Object Drawing.Rectangle 0, 0, $mod.Width, $mod.Height
$bits = $mod.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $samples = 0
    for ($y = 0; $y -lt $mod.Height; $y += 4) {
        for ($x = 0; $x -lt $mod.Width; $x += 4) {
            $px = [Runtime.InteropServices.Marshal]::ReadInt32($bits.Scan0, $y * $bits.Stride + $x * 4)
            $meanLuma += (Get-Channel $px 16) + (Get-Channel $px 8) + (Get-Channel $px 0)
            $samples++
        }
    }
    $meanLuma = ($meanLuma / (3.0 * [Math]::Max($samples, 1)))
} finally { $mod.UnlockBits($bits) }

if ($meanLuma -lt 5.0) {
    Write-Error "Modified view is nearly black (mean luma=$meanLuma) - blur pass failed"
}

Write-Output "Blur visual proof passed."
exit 0
