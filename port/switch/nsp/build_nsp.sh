#!/usr/bin/env bash
# Packs the Switch build into an installable NSP (title 0100D0D0600A0000) with the extracted disc in its RomFS.
# Run in WSL as root after switch/build.sh. Needs the user's own prod.keys (Lockpick_RCM); never commit or share them.
# Usage: build_nsp.sh <prod.keys>     Output: /mnt/c/.../port/switch/nsp/out/*.nsp (~8.3 GB, contains game data:
# for the user's own console only).
set -euo pipefail
KEYS="${1:?usage: build_nsp.sh <path to prod.keys>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
DKP=/opt/devkitpro
ELF="${ELF:-/root/gow-switch-build/ps2xRuntime/ps2EntryRunner.elf}"
PACK="${PACK:-/root/nsp-tools/hacBrewPack/hacbrewpack}"
TITLE=0100D0D0600A0000
WORK=/root/gow-nsp
export PATH="$DKP/tools/bin:$PATH"

[ -f "$KEYS" ] || { echo "error: no keys at $KEYS" >&2; exit 1; }
[ -f "$ELF" ] || { echo "error: no Switch build at $ELF (switch/build.sh)" >&2; exit 1; }
[ -x "$PACK" ] || { echo "error: hacBrewPack not built at $PACK (docs/switch.md)" >&2; exit 1; }

rm -rf "$WORK"
mkdir -p "$WORK/exefs" "$WORK/control"
elf2nso "$ELF" "$WORK/exefs/main"
npdmtool "$HERE/gow.json" "$WORK/exefs/main.npdm"
nacptool --create "God of War" "Waffle565" "0.1.0" "$WORK/control/control.nacp"
cp "$DKP/libnx/default_icon.jpg" "$WORK/control/icon_AmericanEnglish.dat"

# RomFS = disc-usa without the memory card (the card lives on the SD card: sdmc:/switch/gow/play.mcd). The tree is
# made of NTFS hard links (romfs/disc-usa) so the 8 GB of PAKs are not copied.
ROMFS="$HERE/romfs"
[ -d "$ROMFS/disc-usa" ] || { echo "error: run make_romfs_links.sh on Windows first" >&2; exit 1; }

# hacBrewPack aborts on any value that is not 32 or 64 hex digits (newer Lockpick dumps have 34-digit *_kek_source
# entries and RSA keypairs it does not need): give it a filtered copy, deleted after packing.
KEYS_OK="$WORK/prod.keys"
tr -d '\r' < "$KEYS" | awk -F' *= *' 'NF==2 && (length($2)==32 || length($2)==64)' > "$KEYS_OK"
trap 'rm -f "$KEYS_OK"' EXIT

cd "$WORK"
"$PACK" -k "$KEYS_OK" --titleid "$TITLE" --titlename "God of War" --titlepublisher "Waffle565" \
    --exefsdir "$WORK/exefs" --romfsdir "$ROMFS" --controldir "$WORK/control" --nologo \
    --tempdir "$WORK/temp" --ncadir "$WORK/nca" --backupdir "$WORK/backup" --nspdir "$HERE/out"
ls -la "$HERE/out"
