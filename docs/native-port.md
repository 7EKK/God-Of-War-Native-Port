# Native port plan: replacing every emulated layer

Goal (user, 2026-10-10): no emulation. 1920x1080 internal, 60 fps minimum, in every zone.
This file is the inventory and the order of work. Status per layer is measured, not hoped.

## What is "emulated" today

| Layer | Today | Size | Native replacement | Status |
|---|---|---|---|---|
| EE CPU (MIPS R5900 game code) | **static recompilation** to x86 (6.4k functions) — already native code, no interpreter | generated | keep; optimise codegen (drop per-insn `ctx->pc`, fast memory paths) | native |
| Kernel/syscalls/threads | HLE in the runtime (Kernel/, ~23k lines) | HLE | keep | native (HLE) |
| VU0 macro mode | recompiled inline COP2 ops | inline | keep | native |
| VU1 microcode | compiled per instruction pair (+ interpreter fallback for pairs not captured) | ~3.4k lines + generated | **100 % ahead-of-time coverage, no interpreter fallback**, then lower the compiled program to GPU compute (see below) | partial: captured 3400 micro-memories, regeneration in progress |
| VIF1 / GIF / DMA plumbing | software model of the DMA chain, UNPACK into a 16 KB VU memory, GIF packet parser | ~1.5k lines | needed as long as the game feeds VU1/GS in PS2 formats; becomes a thin loader once VU1 outputs vertices directly | emulated plumbing |
| GS (graphics synthesizer) | VRAM-faithful rasteriser on the GPU (compute + hardware raster), 512x448, 4 MB VRAM model | ~11k lines | **native renderer**: real textures, real render targets, scaled to 1080p, no VRAM model | emulated |
| IOP (I/O CPU + IRX modules: CDVD, SIO2, 989snd) | R3000 interpreter running the game's own IRX modules | ~12.5k lines | HLE: native CDVD (file reads from `disc-usa/`), native pad, native sound engine replacing 989snd | emulated |
| SPU2 | software mixer driven by IOP register writes | in IOP | native mixer fed directly by a native sound engine | emulated |

## What a no-emulation port really requires

The EE side is done the way static recompilation ports are done. The remaining emulation is *hardware*, and the only way to
remove hardware emulation is to replace the hardware interface with native code that produces the same result from the
game's own data. For each layer:

1. **VU1 -> GPU vertex processing.** The ~500 microprograms (skinning, transform, lighting, particles) are compiled
   mechanically, not hand-written: lower the compiled VU1 programs (already analysed per instruction pair by
   `generar_vu1`) to GLSL compute, one invocation per draw batch (MSCAL), input = the batch's 16 KB data memory snapshot,
   output = vertex records. This removes the VU1 bucket (the largest cost: 14-33 ms/frame) from the CPU entirely.
2. **GS -> native renderer.** Consume the vertex records and the game's render state (alpha, Z, fog, texture, CLUT,
   render-target switches) and draw with real GPU textures/targets at 1920x1080. The hard part is the game's framebuffer
   feedback passes (channel-shuffle fog, bloom, depth tricks): each must be recognised and replaced by an equivalent pass.
   This is the PCSX2 "hardware renderer" problem, solved per game instead of per console.
3. **IOP -> native services.** CDVD: map the game's file requests to the extracted disc files. Pad: already native.
   989snd: replace the IOP module with a native sound engine (SMPD/SDBK formats are documented in game-internals.md).
4. **Plumbing last.** Once VU1 and GS are native, DMA/VIF/GIF become a loader that hands buffers to those two.

## Order and what unblocks what

1. VU1 interpreter fallback -> 0 (coverage). *Prerequisite for everything below; in progress.*
2. VU1 -> GPU compute (frees ~60 % of the frame time; gets 60 fps in the heavy zones).
3. Native renderer at 1080p (replaces GS; the biggest piece of work).
4. Native sound + CDVD (removes the IOP interpreter; ~19 ms/frame of CPU in Olympus today).
5. EE codegen optimisation (24 ms/frame in Olympus; needs ~2x for 60 fps with margin).

## Honest scale

Steps 2-4 are each a project of weeks, not a session. Nothing here is hypothetical: every layer has a measured cost
and a defined native replacement; none can be skipped to reach "no emulation".

## GS inventory (2026-10-10, `GOW_GS_INVENTORY=1`, Olympus walk, per 5 s)
~1.5 M draws/5 s (≈ 5 k/frame, almost all triangle strips with ABE+ATEST+ZTE), 98 % sample plain uploaded textures (PSMT8/PSMT4 with CLUT);
~860 host→local uploads (8.4 M px); **zero** CPU VRAM reads/writes/snapshots, zero local→host / local→local transfers, no uploads into render
targets, no fog/DATE/dither/PABE/SCANMSK/depth-as-texture. Special cases are rare: RT used as texture (same format) ≈ 6.9 k, feedback
(FBP==TBP) ≈ 4.6 k, CLUT loaded from an RT ≈ 2.8 k of 8.9 k loads, FBMSK≠0 ≈ 18 k, RT reinterpretation (CT16/CT32 → CT24) ≈ 2.3 k (may
include false positives of the span heuristic). => A render-target renderer is feasible with: scaled RT + depth per (FBP,FBW,PSM), texture
cache decoded from a shadow VRAM, CLUT-aware palettised textures, shader alpha test + blend, FBMSK via write mask, and a conservative
downscale-readback for the rare RT-as-CLUT/reinterpret cases.
Plan (seam = new `GSRasterBackend`, old backend stays default/oracle): (a) scale-1 RT renderer vs old backend frames, (b) RT-as-texture,
(c) conservative readback, (d) raise scale to window size (non-uniform 3.75 x 2.41 per axis is fine: vertex scale only), then drop the
bicubic upscale.
