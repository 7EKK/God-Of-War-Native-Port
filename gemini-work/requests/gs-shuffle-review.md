# Task: how our GS backends handle 16-bit (PSMCT16/16S) draws into a 32-bit (PSMCT32) framebuffer with FBMSK

Read-only. Use read_file only (no shell commands). Do not write code.

Context: God of War draws full-screen ctx1 SPRITE passes with FRAME.PSM = 0x2 (PSMCT16) over memory that is
really a PSMCT32 framebuffer, with FRAME.FBMSK = 0x00003FFF or 0xFFFF00FF, textured from the same buffer
(texture PSM also 16-bit). On real hardware / PCSX2 these are "channel shuffle" effects (PCSX2 detects them in
GSRendererHW: search ChannelShuffle, "shuffle", m_channel_shuffle, IsSwizzleMaskFromPSM, TryGSMemClear). Our
image shows a purple/lavender haze where these passes run.

Files (ours):
- ../ext/PS2Recomp-kl/ps2xRuntime/src/lib/gs/gpu/gs_gpu_backend.cpp and gpu/gs_gpu_shaders.cpp (OpenGL backend)
- ../ext/PS2Recomp-kl/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp (CPU backend, FBMSK around lines 1090-1200, 1930-1960)
- ../ext/PS2Recomp-kl/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp (PSM address swizzle)
PCSX2 reference: ../ref/pcsx2-src/pcsx2/GS/Renderers/HW/GSRendererHW.cpp, GSTextureCache.cpp,
../ref/pcsx2-src/pcsx2/GS/Renderers/SW/GSDrawScanline*.cpp (software path is the bit-exact reference).

Questions:
1. In the GPU backend: when FRAME.PSM is CT16 but the memory was last written as CT32, how are pixels addressed,
   converted (5551 packing) and masked with FBMSK? Is the 16-bit FBMSK applied as the low 16 bits (PCSX2 SW: for
   16-bit PSMs fm is converted with the 5551 layout, see GSDrawScanline / GSRendererSW "fm" conversion)?
2. Same for the CPU backend: does it write 16-bit pixels at PSMCT16 addresses (GSLocalMemory semantic, i.e. a
   16-bit write lands inside some 32-bit pixel's low or high half according to the CT16 swizzle), and is the
   texture read of a CT16 view of CT32 data done through the CT16 address swizzle?
3. Any difference vs PCSX2 SW that would turn those passes into a coloured haze (wrong half, wrong mask polarity,
   alpha bit, TEXA expansion of 16-bit texels: TA0/TA1/AEM).
Output: max 40 lines. For each finding: our file:line, PCSX2 file:line, short explanation. Mark UNVERIFIED claims.
