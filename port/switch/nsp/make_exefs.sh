#!/usr/bin/env bash
# WSL: unpacked title for Citron tests without keys (port/switch/nsp/exefs/main + main.npdm next to romfs/).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH=/opt/devkitpro/tools/bin:$PATH
mkdir -p "$HERE/exefs"
elf2nso "${ELF:-/root/gow-switch-build/ps2xRuntime/ps2EntryRunner.elf}" "$HERE/exefs/main"
npdmtool "$HERE/gow.json" "$HERE/exefs/main.npdm"
ls -la "$HERE/exefs"
