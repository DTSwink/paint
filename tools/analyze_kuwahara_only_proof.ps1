$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Get-Channel { param([int]$Px, [int]$Shift) return ($Px -shr $Shift) -band 255 }

function Get-MeanAbsDiff {
    param([Drawing.Bitmap]$A, [Drawing.Bitmap]$B, [int]$Step = 2)
    $rect = New-Object Drawing.Rectangle 0, 0, $A.Width, $A.Height
    $da = $A.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $db = $B.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $total = 0.0; $changed = 0; $count = 0
        for ($y = 0; $y -lt $A.Height; $y += $Step) {
            for ($x = 0; $x -lt $A.Width; $x += $Step) {
                $ia = [Runtime.InteropServices.Marshal]::ReadInt32($da.Scan0, $y * $da.Stride + $x * 4)
                $ib = [Runtime.InteropServices.Marshal]::ReadInt32($db.Scan0, $y * $db.Stride + $x * 4)
                $d = ([Math]::Abs((Get-Channel $ia 16)-(Get-Channel $ib 16)) + [Math]::Abs((Get-Channel $ia 8)-(Get-Channel $ib 8)) + [Math]::Abs((Get-Channel $ia 0)-(Get-Channel $ib 0))) / 3.0
                $total += $d; if ($d -gt 3) { $changed++ }; $count++
            }
        }
        return @{ Mean = $total / $count; ChangedPct = 100.0 * $changed / $count }
    } finally { $A.UnlockBits($da); $B.UnlockBits($db) }
}

function Get-SobelEdgeScore {
    param([Drawing.Bitmap]$Bmp, [int]$Step = 2)
    $w = $Bmp.Width; $h = $Bmp.Height
    $rect = New-Object Drawing.Rectangle 0, 0, $w, $h
    $bits = $Bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $sum = 0.0; $count = 0
        for ($y = 1; $y -lt ($h - 1); $y += $Step) {
            for ($x = 1; $x -lt ($w - 1); $x += $Step) {
                $read = { param($ox,$oy)
                    $px = [Runtime.InteropServices.Marshal]::ReadInt32($bits.Scan0, ($y+$oy)*$bits.Stride + ($x+$ox)*4)
                    (Get-Channel $px 16)+(Get-Channel $px 8)+(Get-Channel $px 0)
                }
                $tl=&$read -1 -1;$tc=&$read 0 -1;$tr=&$read 1 -1;$ml=&$read -1 0;$mr=&$read 1 0;$bl=&$read -1 1;$bc=&$read 0 1;$br=&$read 1 1
                $gx=-$tl-2*$ml-$bl+$tr+2*$mr+$br; $gy=-$tl-2*$tc-$tr+$bl+2*$bc+$br
                $sum += [Math]::Sqrt($gx*$gx+$gy*$gy); $count++
            }
        }
        return @{ Score = $sum / [Math]::Max($count,1) }
    } finally { $Bmp.UnlockBits($bits) }
}

function Get-FlatBlockRatio {
    param([Drawing.Bitmap]$Bmp, [int]$Block = 8, [double]$VarianceThreshold = 12.0)
    $w=$Bmp.Width;$h=$Bmp.Height
    $rect = New-Object Drawing.Rectangle 0,0,$w,$h
    $bits = $Bmp.LockBits($rect,[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $flat=0;$total=0
        for ($by=0;$by+$Block -le $h;$by+=$Block) {
            for ($bx=0;$bx+$Block -le $w;$bx+=$Block) {
                $vals=@(); for ($y=0;$y -lt $Block;$y++) { for ($x=0;$x -lt $Block;$x++) {
                    $px=[Runtime.InteropServices.Marshal]::ReadInt32($bits.Scan0,($by+$y)*$bits.Stride+($bx+$x)*4)
                    $vals += (Get-Channel $px 16)+(Get-Channel $px 8)+(Get-Channel $px 0)
                }}
                $mean=($vals|Measure-Object -Average).Average
                $var=0; foreach($v in $vals){$var+=($v-$mean)*($v-$mean)}; $var/=$vals.Count
                if($var -lt $VarianceThreshold){$flat++}; $total++
            }
        }
        return 100.0*$flat/[Math]::Max($total,1)
    } finally { $Bmp.UnlockBits($bits) }
}

function Extract-SplitPanels {
    param([string]$CapturePath, [int]$Sidebar = 380)
    $full = [Drawing.Bitmap]::FromFile($CapturePath)
    $halfW = [int][Math]::Floor(($full.Width - $Sidebar) / 2); $h = $full.Height
    $orig = New-Object Drawing.Bitmap $halfW, $h
    $mod = New-Object Drawing.Bitmap $halfW, $h
    $og=[Drawing.Graphics]::FromImage($orig); $mg=[Drawing.Graphics]::FromImage($mod)
    $og.DrawImage($full,0,0,(New-Object Drawing.Rectangle $Sidebar,0,$halfW,$h),[Drawing.GraphicsUnit]::Pixel)
    $mg.DrawImage($full,0,0,(New-Object Drawing.Rectangle ($Sidebar+$halfW),0,$halfW,$h),[Drawing.GraphicsUnit]::Pixel)
    $og.Dispose();$mg.Dispose();$full.Dispose()
    return @{ Original=$orig; Modified=$mod; Width=$halfW; Height=$h }
}

function New-DiffBitmap {
    param([Drawing.Bitmap]$A,[Drawing.Bitmap]$B,[int]$Scale=10)
    $w=$A.Width;$h=$A.Height
    $diff=New-Object Drawing.Bitmap $w,$h
    $rect=New-Object Drawing.Rectangle 0,0,$w,$h
    $da=$A.LockBits($rect,[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $db=$B.LockBits($rect,[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $dd=$diff.LockBits($rect,[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for($y=0;$y -lt $h;$y++){ for($x=0;$x -lt $w;$x++){
            $ia=[Runtime.InteropServices.Marshal]::ReadInt32($da.Scan0,$y*$da.Stride+$x*4)
            $ib=[Runtime.InteropServices.Marshal]::ReadInt32($db.Scan0,$y*$db.Stride+$x*4)
            $d=[int][Math]::Min(255,(([Math]::Abs((Get-Channel $ia 16)-(Get-Channel $ib 16))+[Math]::Abs((Get-Channel $ia 8)-(Get-Channel $ib 8))+[Math]::Abs((Get-Channel $ia 0)-(Get-Channel $ib 0)))/3.0)*$Scale)
            [Runtime.InteropServices.Marshal]::WriteInt32($dd.Scan0,$y*$dd.Stride+$x*4,(255 -shl 24)-bor($d -shl 16)-bor($d -shl 8)-bor $d)
        }}
    } finally { $A.UnlockBits($da);$B.UnlockBits($db);$diff.UnlockBits($dd) }
    return $diff
}

$Root = Split-Path $PSScriptRoot -Parent
$ProofDir = Join-Path $Root "out\proof"
$BlurCap = Join-Path $ProofDir "kuwahara_proof_blur_only_capture.png"
$AkfCap = Join-Path $ProofDir "kuwahara_proof_blur_akf_capture.png"
if (-not (Test-Path $BlurCap) -or -not (Test-Path $AkfCap)) { throw "Run captures first." }

$blurPanels = Extract-SplitPanels $BlurCap
$akfPanels = Extract-SplitPanels $AkfCap
$orig = $blurPanels.Original; $blurOnly = $blurPanels.Modified; $blurAkf = $akfPanels.Modified

$firstFilter = Get-MeanAbsDiff $orig $blurOnly
$secondFilter = Get-MeanAbsDiff $blurOnly $blurAkf
$combined = Get-MeanAbsDiff $orig $blurAkf
$origEdge = Get-SobelEdgeScore $orig
$blurEdge = Get-SobelEdgeScore $blurOnly
$akfEdge = Get-SobelEdgeScore $blurAkf
$blurFlat = Get-FlatBlockRatio $blurOnly
$akfFlat = Get-FlatBlockRatio $blurAkf

$diffFirst = New-DiffBitmap $orig $blurOnly
$diffSecond = New-DiffBitmap $blurOnly $blurAkf
$w=$blurPanels.Width; $h=$blurPanels.Height
$proof = New-Object Drawing.Bitmap ($w*3+40), ($h*2+80)
$pg=[Drawing.Graphics]::FromImage($proof)
$pg.Clear([Drawing.Color]::FromArgb(255,20,20,24))
$font=New-Object Drawing.Font "Segoe UI",11
$title=New-Object Drawing.Font "Segoe UI",10,[Drawing.FontStyle]::Bold
$brush=[Drawing.Brushes]::White
$pg.DrawString("1st filter (blur/noise): Original | Blur-only | Diff x10",$title,$brush,10,6)
$pg.DrawImage($orig,10,44); $pg.DrawImage($blurOnly,($w+20),44); $pg.DrawImage($diffFirst,($w*2+30),44)
$pg.DrawString(("Mean={0:N2} Changed={1:N1}%" -f $firstFilter.Mean,$firstFilter.ChangedPct),$font,$brush,10,($h+48))
$row2=$h+62
$pg.DrawString("2nd filter (Kuwahara on blur): Blur-only | +Kuwahara | Diff x10",$title,$brush,10,($row2-18))
$pg.DrawImage($blurOnly,10,($row2+18)); $pg.DrawImage($blurAkf,($w+20),($row2+18)); $pg.DrawImage($diffSecond,($w*2+30),($row2+18))
$pg.DrawString(("Mean={0:N2} Changed={1:N1}%" -f $secondFilter.Mean,$secondFilter.ChangedPct),$font,$brush,10,($row2+18+$h+4))
$pg.Dispose()
$ProofPath = Join-Path $ProofDir "kuwahara_only_proof.png"
$proof.Save($ProofPath,[Drawing.Imaging.ImageFormat]::Png)

$report = @"
Kuwahara-only visual proof
==========================
Blur held constant: noiseType=2, noiseAmount=1, noiseScale=80

FIRST filter (Original vs Blur-only Modified):
  Mean pixel diff: $($firstFilter.Mean.ToString('N3'))
  Changed pixels:  $($firstFilter.ChangedPct.ToString('N2'))%
  Edge: $($origEdge.Score.ToString('N2')) -> $($blurEdge.Score.ToString('N2'))
  Flat blocks: $($blurFlat.ToString('N1'))%

SECOND filter (Blur-only vs Blur+Kuwahara Modified):
  Mean pixel diff: $($secondFilter.Mean.ToString('N3'))
  Changed pixels:  $($secondFilter.ChangedPct.ToString('N2'))%
  Edge: $($blurEdge.Score.ToString('N2')) -> $($akfEdge.Score.ToString('N2'))
  Flat blocks: $($blurFlat.ToString('N1'))% -> $($akfFlat.ToString('N1'))%

COMBINED (Original vs Blur+Kuwahara — previous split_proof):
  Mean pixel diff: $($combined.Mean.ToString('N3'))
  Changed pixels:  $($combined.ChangedPct.ToString('N2'))%
"@
Set-Content (Join-Path $ProofDir "kuwahara_only_report.txt") $report -Encoding UTF8
Write-Output $report

$orig.Dispose();$blurOnly.Dispose();$blurAkf.Dispose();$diffFirst.Dispose();$diffSecond.Dispose();$proof.Dispose()
