# Status detail

Generated from `data.toml` by `tools/status_map.py`; do not edit by hand.

## PS2 hardware → native code (91% done)

| Group | Component | State |
|---|---|---|
| EE | R5900 CPU: game code recompiled to C++ (6.4k functions) | ✅ native |
| EE | Kernel: threads, semaphores, alarms, syscalls (native HLE) | ✅ native |
| EE | FPU and MMI instructions (PMULTH miscompile fixed) | ✅ native |
| EE | DMA controller: no model, the game's packets go straight to VIF/GIF | ✅ native |
| EE | INTC: not touched by game code in gameplay | ✅ native |
| EE | Timers T0/T1/T2: still register-level (game clock, kernel alarms); native clock in progress | 🔧 partial |
| VU / GS | VU1 microcode: ~500 microprograms compiled ahead of time to C++ | ✅ native |
| VU / GS | VU0 macro mode: inline recompiled COP2 | ✅ native |
| VU / GS | VIF1 / GIF: the game's packets decoded directly (UNPACK, GIF tags) | ✅ native |
| VU / GS | GS renderer: native OpenGL render targets, 2x internal, 16:9, mask/shuffle passes | ✅ native |
| VU / GS | GS textures and CLUT: native texture store (no VRAM model) | ✅ native |
| VU / GS | Palettes loaded from render targets: still a readback into GS memory (in progress) | 🔧 partial |
| VU / GS | GS display registers (PMODE/DISPFB/DISPLAY/CSR): native display state | ✅ native |
| VU / GS | IPU: FMV video (skipped for now) | ⏳ pending |
| IOP | R3000A CPU: the game's 8 IRX modules compiled ahead of time to C++ | ✅ native |
| IOP | IOP kernel imports (native HLE) | ✅ native |
| IOP | SIF: RPC and DMA as direct native calls | ✅ native |
| IOP | SPU2: native sound engine driven by LIBSD's exports | ✅ native |
| IOP | SIO2: native pad and memory-card services (chip model deleted) | ✅ native |
| IOP | CDVD: still reads the ISO; extracted game files are the last step | 🔧 partial |

## Game (73% done)

| Group | Component | State |
|---|---|---|
| Flow | Boot, menu, new game | ✅ native |
| Flow | Load and save on the memory card (save test passes) | ✅ native |
| Flow | FMV cutscenes (skipped with GOW_SKIP_FMV) | ⏳ pending |
| Zones (save slots) | Olympus: correct, 60 fps | ✅ native |
| Zones (save slots) | Desert of Lost Souls: correct, 60 fps | ✅ native |
| Zones (save slots) | Cliffs of Madness: correct, 30 fps | 🔧 partial |
| Zones (save slots) | Save slot 4: correct, 60 fps | ✅ native |
| Output | 1920x1080 window, 16:9 | ✅ native |
| Output | Audio (native mixer, 48 kHz) | ✅ native |
| Output | Game speed: exact at 30 fps, 2-3 % fast at 60 fps | 🔧 partial |
| Output | Nintendo Switch build (after the PC port) | ⏳ pending |
