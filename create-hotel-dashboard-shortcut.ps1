$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$launcherPath = Join-Path $projectRoot "launch-hotel-dashboard.ps1"
$powershellPath = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
$shortcutPath = Join-Path `
    ([Environment]::GetFolderPath("Desktop")) `
    "Haven Hotel Dashboard.lnk"

if (-not (Test-Path -LiteralPath $launcherPath -PathType Leaf)) {
    throw "The launcher script was not found: $launcherPath"
}
if (-not (Test-Path -LiteralPath $powershellPath -PathType Leaf)) {
    throw "Windows PowerShell was not found at its standard system location."
}

if (Test-Path -LiteralPath $shortcutPath) {
    $answer = Read-Host "A Haven Hotel Dashboard shortcut already exists. Replace it? (y/N)"
    if ($answer -notmatch "^(y|yes)$") {
        Write-Host "Shortcut setup cancelled; the existing shortcut was left unchanged."
        exit 0
    }
}

$chromeCandidates = @()
if ($env:ProgramFiles) {
    $chromeCandidates += Join-Path $env:ProgramFiles "Google\Chrome\Application\chrome.exe"
}
if (${env:ProgramFiles(x86)}) {
    $chromeCandidates += Join-Path ${env:ProgramFiles(x86)} "Google\Chrome\Application\chrome.exe"
}
if ($env:LOCALAPPDATA) {
    $chromeCandidates += Join-Path $env:LOCALAPPDATA "Google\Chrome\Application\chrome.exe"
}
$chromePath = $chromeCandidates |
    Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    Select-Object -First 1

try {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $powershellPath
    $shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$launcherPath`""
    $shortcut.WorkingDirectory = $projectRoot
    if ($chromePath) {
        $shortcut.IconLocation = "$chromePath,0"
    }
    $shortcut.Description = "Launch the local Haven Hotel management dashboard in Chrome app mode."
    $shortcut.Save()
    Write-Host "Created desktop shortcut: $shortcutPath"
    Write-Host "The shortcut will check MariaDB and start the existing web server when opened."
} catch {
    Write-Host "Could not create the desktop shortcut: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
