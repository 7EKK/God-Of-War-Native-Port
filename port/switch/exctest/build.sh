#!/usr/bin/env bash
# Builds exctest.nro (C++ exception + thread-count probe, same compile/link flags as the game) into Citron's SD card.
set -euo pipefail
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/tools/bin:$DEVKITPRO/devkitA64/bin:$PATH
cd "$(dirname "$0")"
aarch64-none-elf-g++ -O2 -std=c++20 -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -ftls-model=local-exec -ffunction-sections -fdata-sections -fPIC -D__SWITCH__ \
    -I$DEVKITPRO/libnx/include -c main.cpp -o /root/exctest.o
aarch64-none-elf-g++ -L$DEVKITPRO/libnx/lib -fPIE -specs=$DEVKITPRO/libnx/switch.specs -o /root/exctest.elf /root/exctest.o -lnx -lpthread -lm
nacptool --create exctest Waffle565 0.1 /root/exctest.nacp
elf2nro /root/exctest.elf /root/exctest.nro --nacp=/root/exctest.nacp
cp /root/exctest.nro "/mnt/c/Users/lovas/AppData/Roaming/citron/sdmc/switch/gow/exctest.nro"
