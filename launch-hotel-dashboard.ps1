$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$serverExecutable = Join-Path $projectRoot "hotel_web_ucrt64.exe"
$dashboardUrl = "http://127.0.0.1:8080"
$healthUrl = "$dashboardUrl/api/health"

function Test-TcpPort {
    param([string]$Address, [int]$Port)

    $client = New-Object System.Net.Sockets.TcpClient
    $connectResult = $null
    try {
        $connectResult = $client.BeginConnect($Address, $Port, $null, $null)
        if (-not $connectResult.AsyncWaitHandle.WaitOne(1200)) {
            return $false
        }
        $client.EndConnect($connectResult)
        return $client.Connected
    } catch {
        return $false
    } finally {
        if ($connectResult) {
            $connectResult.AsyncWaitHandle.Close()
        }
        $client.Close()
    }
}

function Test-RunningDashboard {
    try {
        $response = Invoke-WebRequest -Uri $healthUrl -UseBasicParsing -TimeoutSec 2
        if ($response.StatusCode -ne 200) {
            return $false
        }
        return ($response.Content | ConvertFrom-Json).status -eq "ok"
    } catch {
        return $false
    }
}

function Find-Chrome {
    $candidates = @()
    $registryPaths = @(
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe",
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe"
    )
    foreach ($registryPath in $registryPaths) {
        if (Test-Path -LiteralPath $registryPath) {
            try {
                $candidates += (Get-Item -LiteralPath $registryPath).GetValue("")
            } catch {
                continue
            }
        }
    }

    if ($env:ProgramFiles) {
        $candidates += Join-Path $env:ProgramFiles "Google\Chrome\Application\chrome.exe"
    }
    if (${env:ProgramFiles(x86)}) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} "Google\Chrome\Application\chrome.exe"
    }
    if ($env:LOCALAPPDATA) {
        $candidates += Join-Path $env:LOCALAPPDATA "Google\Chrome\Application\chrome.exe"
    }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

try {
    if (-not (Test-TcpPort "127.0.0.1" 3306)) {
        Write-Host "MariaDB is not responding at 127.0.0.1:3306." -ForegroundColor Yellow
        Write-Host "Start MySQL/MariaDB in the XAMPP Control Panel, then run this launcher again."
        exit 1
    }

    $dashboardAlreadyRunning = Test-RunningDashboard
    if (-not $dashboardAlreadyRunning) {
        if (Test-TcpPort "127.0.0.1" 8080) {
            throw "Port 8080 is already in use, but it did not respond as the Hotel Management web server. No process was stopped."
        }
        if (-not (Test-Path -LiteralPath $serverExecutable -PathType Leaf)) {
            throw "The web executable was not found. Build it using the 'Build Hotel Web Dashboard' VS Code task or the instructions in web\BUILD.md."
        }
        $ucrt64Bin = "C:\msys64\ucrt64\bin"
        if (Test-Path -LiteralPath $ucrt64Bin -PathType Container) {
            $env:Path = "$ucrt64Bin;$env:Path"
        }

        $stateDirectory = Join-Path $env:LOCALAPPDATA "HavenHotelDashboard"
        New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
        $runId = Get-Date -Format "yyyyMMdd-HHmmss-fff"
        $stdoutPath = Join-Path $stateDirectory "server-$runId.out.log"
        $stderrPath = Join-Path $stateDirectory "server-$runId.err.log"
        $statePath = Join-Path $stateDirectory "server-process.json"

        $serverProcess = Start-Process `
            -FilePath $serverExecutable `
            -WorkingDirectory $projectRoot `
            -WindowStyle Hidden `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -PassThru

        $processInfo = Get-Process -Id $serverProcess.Id -ErrorAction SilentlyContinue
        if (-not $processInfo) {
            throw "The web server exited during startup. Check the latest log files in $stateDirectory."
        }
        $processState = @{
            processId = $serverProcess.Id
            executable = [System.IO.Path]::GetFullPath($serverExecutable)
            startTimeUtcTicks = $processInfo.StartTime.ToUniversalTime().Ticks
            stdout = $stdoutPath
            stderr = $stderrPath
        }
        $processState | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8

        $ready = $false
        for ($attempt = 0; $attempt -lt 30; $attempt++) {
            if (Test-RunningDashboard) {
                $ready = $true
                break
            }
            if (-not (Get-Process -Id $serverProcess.Id -ErrorAction SilentlyContinue)) {
                break
            }
            Start-Sleep -Milliseconds 500
        }
        if (-not $ready) {
            $errorLines = @()
            if (Test-Path -LiteralPath $stderrPath) {
                $errorLines = Get-Content -LiteralPath $stderrPath -Tail 12
            }
            Write-Host "The web server did not become ready at $dashboardUrl." -ForegroundColor Red
            Write-Host "Logs: $stdoutPath and $stderrPath"
            if ($errorLines.Count -gt 0) {
                $errorLines | ForEach-Object { Write-Host $_ }
            }
            exit 1
        }
    } else {
        Write-Host "The Hotel Management web server is already responding; no duplicate was started."
    }

    $chromePath = Find-Chrome
    if (-not $chromePath) {
        throw "Google Chrome was not found in its standard Windows installation locations. The server remains available at $dashboardUrl."
    }
    Start-Process -FilePath $chromePath -ArgumentList @("--app=$dashboardUrl")
    Write-Host "Opened Haven Hotel in Chrome app mode: $dashboardUrl"
} catch {
    Write-Host "Hotel dashboard launch failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
