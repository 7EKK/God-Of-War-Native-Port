#!/usr/bin/env bash
# Builds tools/switch_throttle/switch_throttle.exe with the MSVC build tools (via build.bat).
set -eu
cd "$(dirname "$0")"
powershell -NoProfile -Command "cmd /c .\build.bat"
