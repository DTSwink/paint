param(
    [Parameter(Mandatory = $true)]
    [string]$ShaderRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Error "ShaderViewer executable not found: $Exe"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$vsCso = Join-Path $OutputDir "material_vs.cso"
$psCso = Join-Path $OutputDir "material_ps.cso"
$vsSrc = Join-Path $ShaderRoot "material_vs.hlsl"
$psSrc = Join-Path $ShaderRoot "material_ps.hlsl"
$commonSrc = Join-Path $ShaderRoot "modifier_common.hlsl"

if ((Test-Path $vsCso) -and (Test-Path $psCso) -and (Test-Path $vsSrc) -and (Test-Path $psSrc)) {
    $builtUtc = (Get-Item $psCso).LastWriteTimeUtc
    $srcUtc = @(
        (Get-Item $vsSrc).LastWriteTimeUtc,
        (Get-Item $psSrc).LastWriteTimeUtc,
        (Get-Item (Join-Path $ShaderRoot "common.hlsli")).LastWriteTimeUtc,
        (Get-Item (Join-Path $ShaderRoot "normal_modifier.hlsl")).LastWriteTimeUtc,
        (Get-Item (Join-Path $ShaderRoot "normal_blur_ps.hlsl")).LastWriteTimeUtc
    )
    if (Test-Path $commonSrc) {
        $srcUtc += (Get-Item $commonSrc).LastWriteTimeUtc
    }
    $newestSrc = ($srcUtc | Measure-Object -Maximum).Maximum
    if ($newestSrc -le $builtUtc) {
        Write-Output "Precompiled shaders are up to date."
        exit 0
    }
}

& $Exe "--export-shaders=$OutputDir" "--shader-root=$ShaderRoot"
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Shader export failed; startup will compile shaders on first run."
    exit 0
}

Write-Output "Exported precompiled shaders to $OutputDir"
