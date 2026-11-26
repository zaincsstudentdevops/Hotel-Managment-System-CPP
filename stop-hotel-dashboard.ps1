$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$expectedExecutable = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot "hotel_web_ucrt64.exe")
)
$statePath = Join-Path $env:LOCALAPPDATA "HavenHotelDashboard\server-process.json"

if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
    Write-Host "No launcher-owned server process is recorded."
    Write-Host "If you started the server in a terminal or VS Code, stop it there with Ctrl+C."
    exit 0
}

try {
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $processId = [int]$state.processId
    $recordedExecutable = [System.IO.Path]::GetFullPath([string]$state.executable)
    if ($recordedExecutable -ine $expectedExecutable) {
        throw "The recorded executable does not match this project. The process was left running."
    }

    $process = Get-Process -Id $processId -ErrorAction SilentlyContinue
    if (-not $process) {
        Remove-Item -LiteralPath $statePath -Force
        Write-Host "The recorded server process has already stopped; stale launcher state was removed."
        exit 0
    }

    if (-not $process.Path -or
        [System.IO.Path]::GetFullPath($process.Path) -ine $expectedExecutable) {
        throw "The recorded process ID now belongs to a different executable. It was left running."
    }

    $actualStartTicks = $process.StartTime.ToUniversalTime().Ticks
    $recordedStartTicks = [long]$state.startTimeUtcTicks
    if ([Math]::Abs($actualStartTicks - $recordedStartTicks) -gt [TimeSpan]::FromSeconds(2).Ticks) {
        throw "The recorded process ID has a different start time. It was left running."
    }

    Stop-Process -Id $processId -ErrorAction Stop
    Wait-Process -Id $processId -Timeout 8 -ErrorAction SilentlyContinue
    if (Get-Process -Id $processId -ErrorAction SilentlyContinue) {
        throw "The tracked server process did not stop. No other process was affected."
    }
    Remove-Item -LiteralPath $statePath -Force
    Write-Host "Stopped the launcher-started Hotel Management web server (PID $processId)."
    Write-Host "Chrome and MariaDB were not stopped."
} catch {
    Write-Host "Hotel dashboard stop failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
