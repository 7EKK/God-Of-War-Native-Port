# Native port plan: replacing every emulated layer

Goal (user, 2026-10-10): no emulation. 1920x1080 internal, 60 fps minimum, in every zone.
End target (user, 2026-10-11): the same native port running on the Nintendo Switch: every native layer must stay portable
(AArch64, switch-mesa GL, Tegra X1 CPU ~5-8x slower than the dev PC; see README.md and switch.md).
This file is the inventory and the order of work. Status per layer is measured, not hoped.

## What is "emulated" today

| Layer | Today | Size | Native replacement | Status |
|---|---|---|---|---|
| EE CPU (MIPS R5900 game code) | **static recompilation** to x86 (6.4k functions) — already native code, no interpreter | generated | keep; optimise codegen (drop per-insn `ctx->pc`, fast memory paths) | native |
| Kernel/syscalls/threads | HLE in the runtime (Kernel/, ~23k lines) | HLE | keep | native (HLE) |
| VU0 macro mode | recompiled inline COP2 ops | inline | keep | native |
| VU1 microcode | native per-instruction-pair code generated ahead of time (tools/vu1/vu1n_gen.py), A/B verified | generated | later: lower to GPU compute | native |
| VIF1 / GIF / DMA plumbing | no DMA controller: sceDmaSend hands the game's packets (its DMA tag lists walked in place) to the VIF decoder (UNPACK into VU memory) and the GIF paths | ~1k lines | becomes a thinner loader once VU1 outputs vertices directly | native (step 2) |
| GS (graphics synthesizer) | **native render-target renderer** `GSHwBackend`, the only renderer: real render targets and textures at `PS2X_GS_HW_SCALE` (default 2), paletted textures as index + palette textures, reinterpretation passes (channel shuffles, aliased Z, CRTC merge) done natively | ~2.3k lines | remaining: replace the GSCpuBackend byte store (uploads/CLUT) with a native texture store | native |
| IOP (I/O CPU + IRX modules: CDVD, SIO2, 989snd) | the game's 8 IRX modules (sio2man, dbcman, sio2d, mc2_d, ds2u_d, libsd, 989nomid, smpd_iop; 52k instructions) **compiled ahead of time** to C++ (tools/iop/iopn_gen.py -> ps2xIOP/src/native); HLE kernel imports; the R3000 interpreter only runs code without a compiled form | generated | replace the hardware models below (SPU2, SIO2, CDVD) with native services | native code, in progress |
| SPU2 | native SoundEngine driven by LIBSD's exports (docs/sound.md) | in IOP | done | native (step 3) |

## Standing order (user, 2026-10-11): convert everything to native, nothing emulated
Native now: EE game code (recompiled), VU0 macro, VU1 microprograms and the IOP IRX modules (compiled ahead of time), GS
rendering (native renderer, the only one), kernel (HLE). Remaining hardware models, converted in this order, each step
verified on the 4 zones (images, fps, audio) before the next:
1. pad / memory card: SIO2 chip model + sio2man/dbcman/sio2d/ds2u_d/mc2_d -> native pad and memory-card services;
   - DONE (2026-10-11): MC2_D's card I/O routines and SIO2D's port probes are native C++ overrides (docs/memory-card.md);
     the SIO2 chip model and the DualShock model are deleted; the pad is read natively on the EE (gow_overrides.cpp).
     Mechanism for all such cuts: IRX function overrides (IopNativeOverride, tools/iop/native_overrides.txt);
2. VIF1 / GIF / DMA data path -> VU1 and GS fed directly from the game's packets;
   - DONE (2026-10-11): no DMA controller model. sceDmaSend hands the game's packets (flat or its DMA tag list, walked in
     place) straight to the VIF decoder / GIF paths (`PS2Memory::sendDma`, `sendDmaChain`) and runs the DMA-end handlers
     at once; the few CHCR stores in game code (boot VIF0 chain 0x1838D0, scratchpad copies) are served the same way.
     Packets are still the game's own formats (VIFcodes, GIF tags): decoding them is reading the game's data;
3. SPU2 register-level voice mixer -> native mixer;
   - DONE (2026-10-11): LIBSD exports are native, driving SoundEngine; SPU2 model and SPU DMA removed (docs/sound.md);
4. SIF (EE <-> IOP RPC/DMA) -> direct native calls;
   - ALREADY NATIVE (checked 2026-10-11): the EE sceSif* functions are runtime functions (Kernel/Stubs/SIF.cpp) and the
     IOP's sifman/sifcmd imports are native (ps2xIOP services/iop_rpc.cpp): an RPC is a direct call of the IOP server
     function with copies in and out, SIF DMA is a copy. No SIF register or SIF DMA channel is accessed (inventory below);
5. GS register / 4 MB memory image -> native texture and CLUT store;
   - Measured (2026-10-11, `GOW_GS_TEXSRC=1`, Olympus + Cliffs, texture decodes per run): PSMT8 textures are read
     almost only from data the game uploaded as CT32 (Olympus 27k of 27k, Cliffs 35k + 3.8k from CT16S uploads), CT24
     textures from CT32 uploads (~4.4k), CT16S from same-format uploads (exact or a part); no texture comes from a local
     copy or a render-target readback. So the store keeps upload words at their GS block addresses (the game's
     "upload as 32 bit, read as 8 bit" packing needs the address math), but needs no rasteriser, framebuffer or
     transfer-readback model;
6. CDVD -> extracted game files (last).

## Hardware access inventory (2026-10-11, `GOW_IO_HIST=150`, boot + Olympus)
What the game's own code still reads or writes as hardware registers (the generated code also marks every constant-address
access with `// MMIO:`; `tools/dis.py ADDR` disassembles the USA build):
- GS privileged registers: CSR (VBlank handler 0x182DE0) and PMODE/DISPFB/DISPLAY/BGCOLOR (0x183748, per frame): step 5.
- EE timers: T0 count (busy-wait 0x17A2A0 and timing, ~50k reads in 150 s), T1, T2 (libkernel 0x299xxx).
- Scratchpad DMA (channels 8/9) in 0x117D10, 0x1188F0, 0x1590F0, 0x159C58, 0x101888 and D_STAT clears: served as copies.
- IPU + its DMA (libmpeg 0x26F228..0x279588, 0x189BD0..0x18A5E0) and D_ENABLER/W: FMV, skipped (GOW_SKIP_FMV).
- INTC mask (libkernel 0x2940B0/0x294140), once per DI/EI.
- Not accessed: VIF/GIF registers, SIF registers, SIF DMA, VIF1/GIF channel registers (except the boot TTE setting).

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

## IOP: ahead-of-time compiled IRX modules (2026-10-10)
- Same idea as the EE: the game's own IOP code runs as native code, not interpreted.
- `core/iop_cpu_exec.h`: the R3000A instruction semantics (`iopExecute`), shared by the interpreter and the generated code.
  The generated code calls it with the instruction word and address as constants, so the compiler removes the decoding and the
  behaviour (load and branch delay slots, exceptions) is identical to the interpreter's.
- Capture: `PS2X_IOP_DUMP_MODULES=<dir>` writes each module's relocated code (`<name>.bin`) and `modules.txt` (name, base,
  text size, FNV-1a hash). The 8 modules load at stable addresses (sio2man 0x10000 ... smpd_iop 0x4C900).
- `tools/iop/iopn_gen.py <dump> ps2xIOP/src/native`: one function per module with a label per instruction; straight-line code
  falls through, after a delay slot it jumps to the static target / fall-through, dynamic jumps go through a switch. It returns
  to the IOP dispatcher at import stubs (`jr ra; addiu $0,$0,n`, served by the HLE kernel), on exceptions, outside the module
  and when the budget (scheduler slice, <= 256 instructions) is used. Threads, interrupts and DMA keep working at those points.
- Runtime (`iop_emulator.cpp`): a module loaded at the generated base with the same hash runs natively, before its start();
  otherwise the log says "no native code for ..." (`PS2X_IOP_NATIVE=0` disables for A/B comparison).
- Generated files are not in git (compiled from the game): regenerate after a module or loader change with
  `PS2X_IOP_DUMP_MODULES=port/iop_mods` + one run to the menu, then `python -I tools/iop/iopn_gen.py port/iop_mods
  ext/PS2Recomp-kl/ps2xIOP/src/native` and a CMake re-configure. Without them the empty registry is linked.
- Specialised C++ per instruction, 512 instructions per function (one function per module, or force-inlining the generic
  semantics, takes MSVC > 30 min and > 12 GB); full build of the 8 modules ~4 min.
- Measured (Olympus, `PS2X_IOP_NATIVE_STATS=1`): 100 % of IOP instructions native, 0 interpreted; the IRX code itself costs
  ~30-70 ms per 5 s.
- The IOP's real cost was the EE calling `runEeCycles` ~3.4 M times/s for ~10 IOP cycles each (full SPU2/timer/scheduler
  service per call): now batched (512 IOP cycles). IOP total 2.8 s -> ~0.2 s per 5 s; boot to Olympus gameplay 95 s -> 20 s.
- Next: SPU2 / SIO2 / CDVD hardware models -> native services (audio output, pad, file reads); then GS default (Cliffs 10 fps).
