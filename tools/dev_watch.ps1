param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [int]$DebounceMs = 800
)

$Root = Split-Path $PSScriptRoot -Parent
$DevRun = Join-Path $PSScriptRoot "dev_run.ps1"

Write-Host "ShaderViewer dev watch — rebuild + relaunch on C++/CMake changes"
Write-Host "HLSL changes still hot-reload inside the running app (no restart needed)"
Write-Host "Watching: $Root\src, $Root\shaders, CMakeLists.txt"
Write-Host "Press Ctrl+C to stop."
Write-Host ""

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $Root
$watcher.IncludeSubdirectories = $true
$watcher.EnableRaisingEvents = $true
$watcher.Filter = "*.*"

$script:pending = $false
$script:timer = New-Object System.Timers.Timer
$script:timer.Interval = $DebounceMs
$script:timer.AutoReset = $false
$script:timer.Add_Elapsed({
    $script:pending = $false
    Write-Host "[dev_watch] Change detected — rebuilding and relaunching..."
    try {
        & $DevRun -Config $Config
        Write-Host "[dev_watch] Ready."
    } catch {
        Write-Host "[dev_watch] Failed: $_"
    }
})

function Should-Trigger($path) {
    $rel = $path.Substring($Root.Length).TrimStart('\', '/').Replace('\', '/')
    if ($rel -match '(^|/)(out|build|\.git)(/|$)') { return $false }
    if ($rel -match '\.(cpp|h|hlsl|hlsli|txt)$') { return $true }
    if ($rel -eq "CMakeLists.txt") { return $true }
    return $false
}

Register-ObjectEvent -InputObject $watcher -EventName Changed -Action {
    $path = $Event.SourceEventArgs.FullPath
    if (-not (Should-Trigger $path)) { return }
    $script:pending = $true
    $script:timer.Stop()
    $script:timer.Start()
} | Out-Null

Register-ObjectEvent -InputObject $watcher -EventName Created -Action {
    $path = $Event.SourceEventArgs.FullPath
    if (-not (Should-Trigger $path)) { return }
    $script:pending = $true
    $script:timer.Stop()
    $script:timer.Start()
} | Out-Null

& $DevRun -Config $Config

try {
    while ($true) { Start-Sleep -Seconds 1 }
} finally {
    $watcher.EnableRaisingEvents = $false
    $watcher.Dispose()
    $script:timer.Dispose()
}
