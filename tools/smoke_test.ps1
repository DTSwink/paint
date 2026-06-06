$ErrorActionPreference = "Stop"
$exe = "c:\Users\singerie\Documents\Cursor\temp\ShaderViewer\out\build\Release\ShaderViewer.exe"
$wd = Split-Path $exe
$psPath = "c:\Users\singerie\Documents\Cursor\temp\ShaderViewer\shaders\material_ps.hlsl"
$log = "c:\Users\singerie\Documents\Cursor\temp\ShaderViewer\out\smoke_test.log"

function Log($msg) {
    $line = "$(Get-Date -Format 'HH:mm:ss') $msg"
    Add-Content -Path $log -Value $line
    Write-Output $line
}

function Is-Running { [bool](Get-Process ShaderViewer -ErrorAction SilentlyContinue) }

function Stop-Viewer {
    cmd /c "taskkill /IM ShaderViewer.exe /F >nul 2>&1"
}

Remove-Item $log -ErrorAction SilentlyContinue
Log "=== ShaderViewer smoke test ==="

& "c:\Users\singerie\Documents\Cursor\temp\ShaderViewer\tools\test_shaders.bat"
if ($LASTEXITCODE -ne 0) { Log "FAIL offline shader compile"; exit 1 }
Log "PASS offline shader compile"

Stop-Viewer
Start-Sleep 1

$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep 3
if (-not (Is-Running)) { Log "FAIL app did not stay running after launch"; exit 1 }
Log "PASS app launch (pid=$($p.Id))"

$backup = Get-Content $psPath -Raw
try {
    Set-Content -Path $psPath -Value ($backup + "`n@@@ INVALID HLSL SYNTAX @@@") -NoNewline
    Start-Sleep 4
    if (-not (Is-Running)) { Log "FAIL app crashed after bad shader"; exit 1 }
    Log "PASS survived bad shader injection"

    Set-Content -Path $psPath -Value $backup -NoNewline
    Start-Sleep 4
    if (-not (Is-Running)) { Log "FAIL app crashed after shader restore"; exit 1 }
    Log "PASS survived shader restore"

    $tweak = $backup + "`n// smoke-test touched"
    Set-Content -Path $psPath -Value $tweak -NoNewline
    Start-Sleep 4
    if (-not (Is-Running)) { Log "FAIL app crashed after shader tweak"; exit 1 }
    Log "PASS survived shader tweak reload"

    Set-Content -Path $psPath -Value $backup -NoNewline
} finally {
    Set-Content -Path $psPath -Value $backup -NoNewline
}

$assets = "c:\Users\singerie\Documents\Cursor\temp\ShaderViewer\assets\test_material"
if (-not (Test-Path "$assets\TestMaterial_BaseColor.png")) { Log "FAIL missing test textures"; exit 1 }
Log "PASS test assets present"

$names = Get-ChildItem $assets -Filter "*.png" | ForEach-Object { $_.BaseName }
if ($names -notcontains "TestMaterial_BaseColor") { Log "FAIL expected test material names"; exit 1 }
Log "PASS test material naming convention"

Log "=== ALL SMOKE TESTS PASSED ==="
Stop-Viewer
exit 0
