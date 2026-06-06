param(
    [Parameter(Mandatory = $true)]
    [string]$TargetExe,

    [string]$WorkingDirectory = "",

    [string]$IconPath = "",

    [string]$ShortcutName = "Shader Viewer.lnk"
)

$ErrorActionPreference = "Stop"

$resolvedExe = Resolve-Path -LiteralPath $TargetExe
if (-not $WorkingDirectory) {
    $WorkingDirectory = Split-Path $resolvedExe.Path
}
if (-not $IconPath) {
    $IconPath = Join-Path (Split-Path $PSScriptRoot -Parent) "resources\app.ico"
}

$desktop = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktop $ShortcutName

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $resolvedExe.Path
$shortcut.WorkingDirectory = $WorkingDirectory
if (Test-Path -LiteralPath $IconPath) {
    $shortcut.IconLocation = "$IconPath,0"
}
$shortcut.Description = "Shader Viewer - material and shader preview tool"
$shortcut.Save()

Write-Output "Updated desktop shortcut:"
Write-Output "  Shortcut: $shortcutPath"
Write-Output "  Target:   $($resolvedExe.Path)"
