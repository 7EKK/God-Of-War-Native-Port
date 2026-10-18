# Copies a code-only update (port/switch/nsp/make_update.sh output) onto a Switch running DBI's "Run MTP responder" (USB):
# exefs/main + main.npdm into sdmc:/atmosphere/contents/0100D0D0600A0000/exefs/ (docs/switch.md "NSP").
# Usage (PowerShell, Switch connected by USB, DBI showing the MTP screen):
#   powershell -ExecutionPolicy Bypass -File tools\switch_push_update.ps1 [<folder holding main and main.npdm>]
# MTP devices have no drive letter, so the copy goes through the Windows shell (the same view as Explorer).
param([string]$Source = (Join-Path $PSScriptRoot '..\port\switch\nsp\out\update\atmosphere\contents\0100D0D0600A0000\exefs'))
$ErrorActionPreference = 'Stop'
$Source = (Resolve-Path $Source).Path
foreach ($name in @('main', 'main.npdm')) {
    if (-not (Test-Path (Join-Path $Source $name))) { throw "missing $name in $Source" }
}

$shell = New-Object -ComObject Shell.Application
$device = $shell.Namespace(17).Items() | Where-Object { $_.Name -match 'Switch|DBI' } | Select-Object -First 1
if (-not $device) { throw "No Switch in 'This PC': open DBI, choose 'Run MTP responder' and connect the USB cable." }
$sd = $device.GetFolder.Items() | Where-Object { $_.Name -match 'SD' } | Select-Object -First 1
if (-not $sd) { throw "No SD card storage under '$($device.Name)'." }

# walk (and create where missing) atmosphere/contents/<title id>/exefs
$folder = $sd.GetFolder
foreach ($part in @('atmosphere', 'contents', '0100D0D0600A0000', 'exefs')) {
    $next = $folder.Items() | Where-Object { $_.Name -eq $part } | Select-Object -First 1
    if (-not $next) {
        $folder.NewFolder($part)
        for ($i = 0; $i -lt 100 -and -not $next; $i++) {
            Start-Sleep -Milliseconds 100
            $next = $folder.Items() | Where-Object { $_.Name -eq $part } | Select-Object -First 1
        }
        if (-not $next) { throw "could not create '$part' on the SD card" }
    }
    $folder = $next.GetFolder
}

$src = $shell.Namespace($Source)
foreach ($name in @('main.npdm', 'main')) {
    $folder.CopyHere($src.ParseName($name), 4 + 16)  # no progress dialog, yes to all (replaces the older file)
    # CopyHere returns at once: wait until the file shows up with its full size
    $size = (Get-Item (Join-Path $Source $name)).Length
    $ok = $false
    for ($i = 0; $i -lt 1200 -and -not $ok; $i++) {
        Start-Sleep -Milliseconds 250
        $item = $folder.Items() | Where-Object { $_.Name -eq $name -or $_.Name -eq [IO.Path]::GetFileNameWithoutExtension($name) } | Select-Object -First 1
        $ok = $item -and $item.Size -eq $size
    }
    if (-not $ok) { throw "copy of $name did not complete" }
    Write-Host "copied $name ($size bytes)"
}
Write-Host 'done: start the game normally (the installed NSP now runs this program)'
