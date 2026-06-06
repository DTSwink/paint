$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Get-MeanAbsDiff {
    param([Drawing.Bitmap]$A, [Drawing.Bitmap]$B, [int]$Step = 4)
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
                $ar = ($ia -shr 16) -band 255; $ag = ($ia -shr 8) -band 255; $ab = $ia -band 255
                $br = ($ib -shr 16) -band 255; $bg = ($ib -shr 8) -band 255; $bb = $ib -band 255
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

$Root = Split-Path $PSScriptRoot -Parent
$ProofDir = Join-Path $Root "out\proof"
New-Item -ItemType Directory -Force -Path $ProofDir | Out-Null

& (Join-Path $PSScriptRoot "create_test_material.ps1") | Out-Null

$Exe = Join-Path $Root "out\build\Release\ShaderViewer.exe"
$Cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$BuildDir = Join-Path $Root "out\build"
& $Cmake --build $BuildDir --config Release --target ShaderViewer
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$CapturePath = Join-Path $ProofDir "split_capture.png"
cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -ArgumentList "--proof-capture=$CapturePath" -PassThru -Wait
if ($p.ExitCode -ne 0) { throw "Proof capture failed with exit code $($p.ExitCode)" }
if (-not (Test-Path $CapturePath)) { throw "Missing capture file: $CapturePath" }

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

$stats = Get-MeanAbsDiff -A $orig -B $mod -Step 2

$diff = New-Object Drawing.Bitmap $halfW, $h
$rect = New-Object Drawing.Rectangle 0, 0, $halfW, $h
$do = $orig.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$dm = $mod.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$dd = $diff.LockBits($rect, [Drawing.Imaging.ImageLockMode]::WriteOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $halfW; $x++) {
            $ia = [Runtime.InteropServices.Marshal]::ReadInt32($do.Scan0, $y * $do.Stride + $x * 4)
            $ib = [Runtime.InteropServices.Marshal]::ReadInt32($dm.Scan0, $y * $dm.Stride + $x * 4)
            $ar = ($ia -shr 16) -band 255; $ag = ($ia -shr 8) -band 255; $ab = $ia -band 255
            $br = ($ib -shr 16) -band 255; $bg = ($ib -shr 8) -band 255; $bb = $ib -band 255
            $d = [int][Math]::Min(255, (([Math]::Abs($ar - $br) + [Math]::Abs($ag - $bg) + [Math]::Abs($ab - $bb)) / 3.0) * 10)
            $px = (255 -shl 24) -bor ($d -shl 16) -bor ($d -shl 8) -bor $d
            [Runtime.InteropServices.Marshal]::WriteInt32($dd.Scan0, $y * $dd.Stride + $x * 4, $px)
        }
    }
} finally {
    $orig.UnlockBits($do); $mod.UnlockBits($dm); $diff.UnlockBits($dd)
}

$proof = New-Object Drawing.Bitmap ($halfW * 3 + 40), ($h + 50)
$pg = [Drawing.Graphics]::FromImage($proof)
$pg.Clear([Drawing.Color]::FromArgb(255, 20, 20, 24))
$font = New-Object Drawing.Font "Segoe UI", 12
$brush = [Drawing.Brushes]::White
$pg.DrawString("Original", $font, $brush, 10, 8)
$pg.DrawString("Modified", $font, $brush, ($halfW + 20), 8)
$pg.DrawString("Diff x10", $font, $brush, ($halfW * 2 + 30), 8)
$pg.DrawImage($orig, 10, 30)
$pg.DrawImage($mod, ($halfW + 20), 30)
$pg.DrawImage($diff, ($halfW * 2 + 30), 30)
$pg.DrawString(("Mean diff={0:N2}  Changed={1:N1}%" -f $stats.Mean, $stats.ChangedPct), $font, $brush, 10, ($h + 32))
$pg.Dispose()

$ProofPath = Join-Path $ProofDir "split_proof.png"
$proof.Save($ProofPath, [Drawing.Imaging.ImageFormat]::Png)
$orig.Dispose(); $mod.Dispose(); $diff.Dispose(); $proof.Dispose()

Write-Output "Capture: $CapturePath"
Write-Output "Proof:   $ProofPath"
Write-Output ("Mean pixel diff: {0:N3}" -f $stats.Mean)
Write-Output ("Changed pixels:  {0:N2}%" -f $stats.ChangedPct)

if ($stats.Mean -lt 1.5 -or $stats.ChangedPct -lt 5.0) {
    Write-Error "Visual difference too small (mean=$($stats.Mean) changed%=$($stats.ChangedPct))"
}
exit 0
