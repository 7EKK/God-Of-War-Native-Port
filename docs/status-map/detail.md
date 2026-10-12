# Status detail

Generated from `data.toml` by `tools/status_map.py`; do not edit by hand.

## Sony SDK libraries (74.4%, 647 functions)

| Library | Functions | State | Note |
|---|---:|---|---|
| libmpeg | 103 | ⏳ pending | FMV decoder; cutscenes are skipped (GOW_SKIP_FMV) for now |
| libipu | 5 | ⏳ pending | IPU driver for FMV; skipped with the cutscenes |
| libgraph | 7 | ✅ ok | Display setup is a native override (GS display state, no GS register access) |
| libdma | 8 | ✅ ok | sceDmaSend feeds VIF/GIF directly; no DMA controller model |
| libcdvd | 10 | 🔧 partial | Reads the original ISO; extracted game files are the last step |
| libdbc | 11 | ✅ ok | dbcman RPC answered by native pad services |
| libpad2 | 13 | ✅ ok | Native pad read on the EE (keyboard / gamepad) |
| libvib | 2 | ⏳ pending | Vibration not implemented |
| libmc2 | 90 | ✅ ok | Native memory-card service; load and save verified (savetest) |
| libscf | 14 | ✅ ok |  |
| libgcc | 32 | ✅ ok |  |
| C++ EH | 34 | ✅ ok |  |
| libm | 14 | ✅ ok |  |
| libc | 150 | ✅ ok |  |
| libkernel | 101 | 🔧 partial | Runs natively; its alarms and the game clock still read the EE timer registers (native clock in progress) |
| sif | 24 | ✅ ok | RPC as direct native calls |
| fileio | 15 | ✅ ok |  |
| loadfile | 14 | ✅ ok |  |

## PS2 hardware (91.9%)

| Group | Component | Weight | State |
|---|---|---:|---|
| EE | R5900 CPU: game code statically recompiled to C++ (6.4k functions) | 5 | ✅ ok |
| EE | Kernel: threads, semaphores, alarms, syscalls (native HLE) | 3 | ✅ ok |
| EE | FPU (COP1) and MMI instructions | 2 | ✅ ok |
| EE | INTC: not accessed by game code in gameplay | 2 | ✅ ok |
| EE | DMA controller: no model, packets go straight to VIF/GIF | 2 | ✅ ok |
| EE | Timers: T2 native (DelayThread on the scheduler); T0/T1 reads still served by the timer model (needs recompiler change) | 1 | 🔧 partial |
| GS / VU | GS: native OpenGL render-target renderer, 2x internal, 16:9; display registers native | 3 | ✅ ok |
| GS / VU | VIF1 decoded directly; VU1 microprograms compiled ahead of time to C++ | 3 | ✅ ok |
| GS / VU | Textures and CLUT: native texture store; palettes from render targets read on the GPU | 2 | ✅ ok |
| GS / VU | GIF (PATH1-3): the game's GIF tags fed to the renderer | 2 | ✅ ok |
| GS / VU | IPU: FMV video (skipped for now) | 2 | ⏳ pending |
| IOP | R3000A CPU: no interpreter on the hot path | 3 | ✅ ok |
| IOP | SPU2: native sound engine driven by LIBSD's exports | 3 | ✅ ok |
| IOP | The game's 8 IRX modules compiled ahead of time to C++ | 2 | ✅ ok |
| IOP | SIF: RPC and DMA as direct native calls | 2 | ✅ ok |
| IOP | CDVD: reads the ISO; extracted files are the last step | 2 | 🔧 partial |
| IOP | Pad: native service (SIO2 chip model deleted) | 2 | ✅ ok |
| IOP | Memory card: native service, load/save verified | 2 | ✅ ok |
