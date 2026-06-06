$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Get-Channel {
    param([int]$Px, [int]$Shift)
    return ($Px -shr $Shift) -band 255
}

function Get-MeanAbsDiff {
    param([Drawing.Bitmap]$A, [Drawing.Bitmap]$B, [int]$Step = 2)
    $rect = New-Object Drawing.Rectangle 0, 0, $A.Width, $A.Height
    $da = $A.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $db = $B.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $total = 0.0
        $changed = 0
        $count = 0
        $threshold = 3
        for ($y = 0; $y -lt $A.Height; $y += $Step) {
            for ($x = 0; $x -lt $A.Width; $x += $Step) {
                $ia = [Runtime.InteropServices.Marshal]::ReadInt32($da.Scan0, $y * $da.Stride + $x * 4)
                $ib = [Runtime.InteropServices.Marshal]::ReadInt32($db.Scan0, $y * $db.Stride + $x * 4)
                $ar = Get-Channel $ia 16; $ag = Get-Channel $ia 8; $ab = Get-Channel $ia 0
                $br = Get-Channel $ib 16; $bg = Get-Channel $ib 8; $bb = Get-Channel $ib 0
                $d = ([Math]::Abs($ar - $br) + [Math]::Abs($ag - $bg) + [Math]::Abs($ab - $bb)) / 3.0
                $total += $d
                if ($d -gt $threshold) { $changed++ }
                $count++
            }
        }
        return @{ Mean = $total / $count; ChangedPct = 100.0 * $changed / $count }
    } finally {
        $A.UnlockBits($da)
        $B.UnlockBits($db)
    }
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
                    return (Get-Channel $px 16) + (Get-Channel $px 8) + (Get-Channel $px 0)
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
                        $vals.Add((Get-Channel $px 16) + (Get-Channel $px 8) + (Get-Channel $px 0))
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

function Set-SettingsAkfStrength {
    param([string]$SettingsPath, [double]$Strength)
    $lines = Get-Content $SettingsPath
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^akfStrength=') {
            $lines[$i] = "akfStrength=$Strength"
            $found = $true
            break
        }
    }
    if (-not $found) { $lines += "akfStrength=$Strength" }
    Set-Content -Path $SettingsPath -Value $lines -Encoding UTF8
}

function Extract-SplitPanels {
    param([string]$CapturePath, [int]$Sidebar = 380)
    $full = [Drawing.Bitmap]::FromFile($CapturePath)
    $vpW = $full.Width - $Sidebar
    $halfW = [int][Math]::Floor($vpW / 2)
    $h = $full.Height
    $orig = New-Object Drawing.Bitmap $halfW, $h
    $mod = New-Object Drawing.Bitmap $halfW, $h
    $og = [Drawing.Graphics]::FromImage($orig)
    $mg = [Drawing.Graphics]::FromImage($mod)
    $og.DrawImage($full, 0, 0, (New-Object Drawing.Rectangle $Sidebar, 0, $halfW, $h), [Drawing.GraphicsUnit]::Pixel)
    $mg.DrawImage($full, 0, 0, (New-Object Drawing.Rectangle ($Sidebar + $halfW), 0, $halfW, $h), [Drawing.GraphicsUnit]::Pixel)
    $og.Dispose(); $mg.Dispose(); $full.Dispose()
    return @{ Original = $orig; Modified = $mod; Width = $halfW; Height = $h }
}

function Invoke-ProofCapture {
    param([string]$Exe, [string]$OutPath)
    cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
    Start-Sleep -Milliseconds 300
    $p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList "--proof-capture=$OutPath" -PassThru -Wait
    if ($p.ExitCode -ne 0) { throw "Capture failed ($OutPath) exit $($p.ExitCode)" }
    if (-not (Test-Path $OutPath)) { throw "Missing capture: $OutPath" }
}

function New-DiffBitmap {
    param([Drawing.Bitmap]$A, [Drawing.Bitmap]$B, [int]$Scale = 10)
    $w = $A.Width; $h = $A.Height
    $diff = New-Object Drawing.Bitmap $w, $h
    $rect = New-Object Drawing.Rectangle 0, 0, $w, $h
    $da = $A.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $db = $B.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $dd = $diff.LockBits($rect, [Drawing.Imaging.ImageLockMode]::WriteOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $h; $y++) {
            for ($x = 0; $x -lt $w; $x++) {
                $ia = [Runtime.InteropServices.Marshal]::ReadInt32($da.Scan0, $y * $da.Stride + $x * 4)
                $ib = [Runtime.InteropServices.Marshal]::ReadInt32($db.Scan0, $y * $db.Stride + $x * 4)
                $ar = Get-Channel $ia 16; $ag = Get-Channel $ia 8; $ab = Get-Channel $ia 0
                $br = Get-Channel $ib 16; $bg = Get-Channel $ib 8; $bb = Get-Channel $ib 0
                $d = [int][Math]::Min(255, (([Math]::Abs($ar - $br) + [Math]::Abs($ag - $bg) + [Math]::Abs($ab - $bb)) / 3.0) * $Scale)
                $px = (255 -shl 24) -bor ($d -shl 16) -bor ($d -shl 8) -bor $d
                [Runtime.InteropServices.Marshal]::WriteInt32($dd.Scan0, $y * $dd.Stride + $x * 4, $px)
            }
        }
    } finally {
        $A.UnlockBits($da); $B.UnlockBits($db); $diff.UnlockBits($dd)
    }
    return $diff
}

$Root = Split-Path $PSScriptRoot -Parent
$ProofDir = Join-Path $Root "out\proof"
New-Item -ItemType Directory -Force -Path $ProofDir | Out-Null

& (Join-Path $PSScriptRoot "create_test_material.ps1") | Out-Null

$Exe = Join-Path $Root "out\build\Release\ShaderViewer.exe"
$Cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $Cmake --build (Join-Path $Root "out\build") --config Release --target ShaderViewer
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$SettingsPath = Join-Path $Root "settings\settings.ini"
if (-not (Test-Path $SettingsPath)) { throw "Missing settings: $SettingsPath" }
$SettingsBackup = Join-Path $ProofDir "settings_backup.ini"
Copy-Item $SettingsPath $SettingsBackup -Force

try {
    # Run 1: blur/noise ON, Kuwahara OFF — Modified side = after first filter only
    Set-SettingsAkfStrength -SettingsPath $SettingsPath -Strength 0
    $CaptureBlurOnly = Join-Path $ProofDir "kuwahara_proof_blur_only_capture.png"
    Invoke-ProofCapture -Exe $Exe -OutPath $CaptureBlurOnly
    $blurPanels = Extract-SplitPanels -CapturePath $CaptureBlurOnly

    # Run 2: blur/noise ON, Kuwahara ON — Modified side = after both filters
    Set-SettingsAkfStrength -SettingsPath $SettingsPath -Strength 1
    $CaptureBlurAkf = Join-Path $ProofDir "kuwahara_proof_blur_akf_capture.png"
    Invoke-ProofCapture -Exe $Exe -OutPath $CaptureBlurAkf
    $akfPanels = Extract-SplitPanels -CapturePath $CaptureBlurAkf

    $orig = $blurPanels.Original
    $blurOnly = $blurPanels.Modified
    $blurAkf = $akfPanels.Modified

    $blurOnlyPath = Join-Path $ProofDir "kuwahara_proof_blur_only_modified.png"
    $blurAkfPath = Join-Path $ProofDir "kuwahara_proof_blur_akf_modified.png"
    $blurOnly.Save($blurOnlyPath, [Drawing.Imaging.ImageFormat]::Png)
    $blurAkf.Save($blurAkfPath, [Drawing.Imaging.ImageFormat]::Png)

    # Metrics
    $firstFilter = Get-MeanAbsDiff -A $orig -B $blurOnly -Step 2
    $secondFilter = Get-MeanAbsDiff -A $blurOnly -B $blurAkf -Step 2
    $combined = Get-MeanAbsDiff -A $orig -B $blurAkf -Step 2

    $origEdge = Get-SobelEdgeScore $orig
    $blurEdge = Get-SobelEdgeScore $blurOnly
    $akfEdge = Get-SobelEdgeScore $blurAkf
    $blurFlat = Get-FlatBlockRatio $blurOnly
    $akfFlat = Get-FlatBlockRatio $blurAkf

    $diffSecond = New-DiffBitmap -A $blurOnly -B $blurAkf -Scale 10
    $diffFirst = New-DiffBitmap -A $orig -B $blurOnly -Scale 10

    $w = $blurPanels.Width
    $h = $blurPanels.Height
    $proof = New-Object Drawing.Bitmap ($w * 3 + 40), ($h * 2 + 80)
    $pg = [Drawing.Graphics]::FromImage($proof)
    $pg.Clear([Drawing.Color]::FromArgb(255, 20, 20, 24))
    $font = New-Object Drawing.Font "Segoe UI", 11
    $title = New-Object Drawing.Font "Segoe UI", 10, [Drawing.FontStyle]::Bold
    $brush = [Drawing.Brushes]::White

    $pg.DrawString("1st filter only (blur, no Kuwahara): Original | Blur-only Modified | Diff x10", $title, $brush, 10, 6)
    $pg.DrawString("Original", $font, $brush, 10, 26)
    $pg.DrawString("Blur only", $font, $brush, ($w + 20), 26)
    $pg.DrawString("Diff x10", $font, $brush, ($w * 2 + 30), 26)
    $pg.DrawImage($orig, 10, 44)
    $pg.DrawImage($blurOnly, ($w + 20), 44)
    $pg.DrawImage($diffFirst, ($w * 2 + 30), 44)
    $pg.DrawString(("Mean={0:N2}  Changed={1:N1}%" -f $firstFilter.Mean, $firstFilter.ChangedPct), $font, $brush, 10, ($h + 48))

    $row2Y = $h + 62
    $pg.DrawString("2nd filter only (Kuwahara on top of blur): Blur-only | Blur+Kuwahara | Diff x10", $title, $brush, 10, ($row2Y - 18))
    $pg.DrawString("Blur only", $font, $brush, 10, $row2Y)
    $pg.DrawString("+ Kuwahara", $font, $brush, ($w + 20), $row2Y)
    $pg.DrawString("Diff x10", $font, $brush, ($w * 2 + 30), $row2Y)
    $pg.DrawImage($blurOnly, 10, ($row2Y + 18))
    $pg.DrawImage($blurAkf, ($w + 20), ($row2Y + 18))
    $pg.DrawImage($diffSecond, ($w * 2 + 30), ($row2Y + 18))
    $pg.DrawString(("Mean={0:N2}  Changed={1:N1}%" -f $secondFilter.Mean, $secondFilter.ChangedPct), $font, $brush, 10, ($row2Y + 18 + $h + 4))
    $pg.Dispose()

    $ProofPath = Join-Path $ProofDir "kuwahara_only_proof.png"
    $proof.Save($ProofPath, [Drawing.Imaging.ImageFormat]::Png)

    $report = @"
Kuwahara-only visual proof
==========================
Blur held constant: noiseType=2 (Gaussian), noiseAmount=1, noiseScale=80

FIRST filter (Original vs Blur-only Modified) — includes noise/blur:
  Mean pixel diff: $($firstFilter.Mean.ToString('N3'))
  Changed pixels:  $($firstFilter.ChangedPct.ToString('N2'))%
  Edge score orig -> blur: $($origEdge.Score.ToString('N2')) -> $($blurEdge.Score.ToString('N2'))
  Flat blocks (blur): $($blurFlat.ToString('N1'))%

SECOND filter (Blur-only vs Blur+Kuwahara Modified) — Kuwahara only:
  Mean pixel diff: $($secondFilter.Mean.ToString('N3'))
  Changed pixels:  $($secondFilter.ChangedPct.ToString('N2'))%
  Edge score blur -> +Kuwahara: $($blurEdge.Score.ToString('N2')) -> $($akfEdge.Score.ToString('N2'))
  Flat blocks blur -> +Kuwahara: $($blurFlat.ToString('N1'))% -> $($akfFlat.ToString('N1'))%

COMBINED (Original vs Blur+Kuwahara) — what split_proof measured (both filters):
  Mean pixel diff: $($combined.Mean.ToString('N3'))
  Changed pixels:  $($combined.ChangedPct.ToString('N2'))%

Files:
  $ProofPath
  $blurOnlyPath
  $blurAkfPath
"@
    $ReportPath = Join-Path $ProofDir "kuwahara_only_report.txt"
    Set-Content -Path $ReportPath -Value $report -Encoding UTF8

    $orig.Dispose()
    $blurOnly.Dispose()
    $blurAkf.Dispose()
    $diffFirst.Dispose()
    $diffSecond.Dispose()
    $proof.Dispose()

    Write-Output $report

    if ($secondFilter.Mean -lt 1.0 -or $secondFilter.ChangedPct -lt 3.0) {
        Write-Error "Kuwahara-only effect too small (mean=$($secondFilter.Mean) changed%=$($secondFilter.ChangedPct))"
    }
} finally {
    Copy-Item $SettingsBackup $SettingsPath -Force
    Remove-Item $SettingsBackup -Force -ErrorAction SilentlyContinue
}

exit 0
