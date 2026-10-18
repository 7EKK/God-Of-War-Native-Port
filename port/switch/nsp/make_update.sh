#!/usr/bin/env bash
# WSL, after switch/build.sh: a code-only update for an installed NSP, no reinstall. Atmosphère's loader takes a
# title's ExeFS from sdmc:/atmosphere/contents/<title id>/exefs/ when that folder exists, so only the program (~35 MB,
# no game data) has to reach the console; the game data stays in the installed NSP's RomFS.
# Output: port/switch/nsp/out/update/atmosphere/... (copy the "atmosphere" folder to the SD card root), and with
# DEST=<dir> also a copy there. Delete sdmc:/atmosphere/contents/0100D0D0600A0000/exefs after installing a newer
# full NSP, or this older program keeps overriding it.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH=/opt/devkitpro/tools/bin:$PATH
TITLE=0100D0D0600A0000
OUT="$HERE/out/update"
EXEFS="$OUT/atmosphere/contents/$TITLE/exefs"
rm -rf "$OUT"
mkdir -p "$EXEFS"
elf2nso "${ELF:-/root/gow-switch-build/ps2xRuntime/ps2EntryRunner.elf}" "$EXEFS/main"
npdmtool "$HERE/gow.json" "$EXEFS/main.npdm"
if [ -n "${DEST:-}" ]; then
    rm -rf "$DEST/atmosphere/contents/$TITLE/exefs"
    mkdir -p "$DEST"
    cp -r "$OUT/atmosphere" "$DEST/"
fi
ls -la "$EXEFS"
