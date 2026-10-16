# Copies the game's log from a Switch running DBI's "Run MTP responder" (USB) into port/switch/logs/<date>/.
# Usage (PowerShell, Switch connected by USB, DBI showing the MTP screen):
#   powershell -ExecutionPolicy Bypass -File tools\switch_pull_log.ps1
# MTP devices have no drive letter, so the copy goes through the Windows shell (the same view as Explorer).
$ErrorActionPreference = 'Stop'
$files = @('log.txt', 'log_prev.txt', 'env.txt')
$dest = Join-Path $PSScriptRoot ("..\port\switch\logs\" + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Force $dest | Out-Null
$dest = (Resolve-Path $dest).Path

$shell = New-Object -ComObject Shell.Application
$thisPc = $shell.Namespace(17)
$device = $thisPc.Items() | Where-Object { $_.Name -match 'Switch|DBI' } | Select-Object -First 1
if (-not $device) { throw "No Switch in 'This PC': open DBI, choose 'Run MTP responder' and connect the USB cable." }

# DBI lists several storages ("1: SD Card", "2: Nand USER", ...): the SD card holds switch/gow.
$sd = $device.GetFolder.Items() | Where-Object { $_.Name -match 'SD' } | Select-Object -First 1
if (-not $sd) { throw "No SD card storage under '$($device.Name)'." }
$folder = $sd.GetFolder
foreach ($part in @('switch', 'gow')) {
    $next = $folder.Items() | Where-Object { $_.Name -eq $part } | Select-Object -First 1
    if (-not $next) { throw "No '$part' folder on the SD card (has the game run once?)." }
    $folder = $next.GetFolder
}

$target = $shell.Namespace($dest)
foreach ($name in $files) {
    $item = $folder.Items() | Where-Object { $_.Name -eq $name -or $_.Name -eq [IO.Path]::GetFileNameWithoutExtension($name) } | Select-Object -First 1
    if (-not $item) { Write-Host "missing on the Switch: $name"; continue }
    $target.CopyHere($item, 4 + 16)  # no progress dialog, yes to all
    # CopyHere returns at once: wait until the file is complete
    $out = Join-Path $dest $name
    for ($i = 0; $i -lt 600 -and -not (Test-Path $out); $i++) { Start-Sleep -Milliseconds 100 }
    Write-Host "copied $name -> $out"
}
Write-Host "done: $dest"
