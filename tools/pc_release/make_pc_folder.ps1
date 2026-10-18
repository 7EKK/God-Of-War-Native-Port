# Builds a self-contained, double-clickable PC folder of the port (docs/build-and-run.md "PC release folder"):
#   <Dest>\God of War.exe   launcher (launcher.cpp, icon from port/switch/nsp/icon.jpg)
#   <Dest>\game\            ps2EntryRunner.exe (Release build) + DLLs
#   <Dest>\disc\            the extracted USA disc from disc-usa\ + play.mcd (memory card)
# Usage: powershell -ExecutionPolicy Bypass -File tools\pc_release\make_pc_folder.ps1 [-Dest <folder>]
# Re-running updates the program and copies only changed disc files; an existing disc\play.mcd (the saves) is kept.
param([string]$Dest = (Join-Path $PSScriptRoot '..\..\dist\God of War (PC)'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$release = Join-Path $root 'ext\PS2Recomp-kl\out\build-kl\ps2xRuntime\Release'
$disc = Join-Path $root 'disc-usa'
if (-not (Test-Path "$release\ps2EntryRunner.exe")) { throw "Release runner not built: $release" }
if (-not (Test-Path "$disc\SCUS_973.99")) { throw "No extracted disc in $disc (docs/disc.md)" }

# launcher, built in a temp folder with the MSVC Build Tools
$work = Join-Path $env:TEMP 'gow_launcher_build'
New-Item -ItemType Directory -Force $work | Out-Null
python -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2], sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])" `
    (Join-Path $root 'port\switch\nsp\icon.jpg') (Join-Path $work 'icon.ico')
if ($LASTEXITCODE -ne 0) { throw 'icon conversion failed (needs Pillow)' }
Set-Content -Encoding ascii (Join-Path $work 'launcher.rc') '1 ICON "icon.ico"'
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$src = Join-Path $PSScriptRoot 'launcher.cpp'
cmd /c "`"$vcvars`" >nul && cd /d `"$work`" && rc /nologo launcher.rc && cl /nologo /std:c++17 /O2 /EHsc /DUNICODE /D_UNICODE `"$src`" launcher.res /Fe:launcher.exe /link /SUBSYSTEM:WINDOWS user32.lib"
if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }

New-Item -ItemType Directory -Force $Dest, "$Dest\game", "$Dest\disc" | Out-Null
Copy-Item -Force (Join-Path $work 'launcher.exe') (Join-Path $Dest 'God of War.exe')

# program: the runner and its DLLs only (no pdb/lib or test output from the build folder)
robocopy $release "$Dest\game" ps2EntryRunner.exe *.dll /PURGE /NJH /NJS /NP /NFL /NDL | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy game failed ($LASTEXITCODE)" }

# disc: the extracted disc without the test memory cards (Mcd001.ps2, play.mcd, mc0/, mc1/). Hard links on the same
# volume (no second 8 GB copy; the disc files are never written), copies elsewhere.
$sameVolume = [IO.Path]::GetPathRoot((Resolve-Path $Dest).Path) -eq [IO.Path]::GetPathRoot($disc)
Get-ChildItem -Recurse -File -Force $disc | Where-Object {
    $_.Name -notin @('Mcd001.ps2', 'play.mcd') -and $_.FullName -notmatch '[\\/](mc0|mc1)[\\/]'
} | ForEach-Object {
    $target = Join-Path "$Dest\disc" $_.FullName.Substring($disc.Length + 1)
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    $old = Get-Item -Force -LiteralPath $target -ErrorAction SilentlyContinue
    if ($old -and $old.Length -eq $_.Length -and $old.LastWriteTime -eq $_.LastWriteTime) { return }
    if ($old) { Remove-Item -Force -LiteralPath $target }
    if ($sameVolume) { New-Item -ItemType HardLink -Path $target -Target $_.FullName | Out-Null }
    else { Copy-Item -LiteralPath $_.FullName $target }
}
if (-not (Test-Path "$Dest\disc\play.mcd")) {
    $card = if (Test-Path "$disc\play.mcd") { "$disc\play.mcd" } elseif (Test-Path "$root\ref\saves\Mcd002.ps2") { "$root\ref\saves\Mcd002.ps2" }
    if ($card) { Copy-Item $card "$Dest\disc\play.mcd" }
}
$size = (Get-ChildItem -Recurse -File $Dest | Measure-Object Length -Sum).Sum / 1GB
Write-Host ("done: {0} ({1:N1} GB) - start 'God of War.exe'" -f $Dest, $size)
