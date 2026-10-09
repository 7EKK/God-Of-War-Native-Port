# Task: research paths to full speed (60 fps) for the God of War PS2 static recompilation

Context: read ../docs/status.md (section "Performance work" and the last entries) and
../ref/other-recomp/god-of-war-recomp-main/docs/RENDIMIENTO.md (Spanish). Current: ~4 game frames/s in gameplay;
per 5 s: VU1 interpreter 3.07 s, EE (recompiled C++) 1.58 s, GS (OpenGL) 0.20 s, IOP 0.15 s.
A per-pair VU1 C++ code generator exists (tools/vu1/generar_vu1.cpp in that project) and is ~2x faster than the
interpreter (cycle-accurate latency model kept).

Answer, with links/source paths for every claim:
1. How PCSX2's microVU (../ref/pcsx2-src/pcsx2/x86/microVU*.{h,cpp,inl}) avoids
   per-instruction pipeline bookkeeping: block-level analysis of stalls, flag instances (status/mac/clip),
   Q/P pipelines, XGKICK timing. Summarize the minimum state a block-level static VU1 compiler must track to
   stay correct, and what microVU skips (e.g. flag-hack, unused flags).
2. Can VU1 run on its own host thread in parallel with the EE (PCSX2 "MTVU")? What synchronization does MTVU
   need (VIF1 FIFO, XGKICK/PATH1, VU1 registers read by EE via VU0 CFC2/VIF)? Relevance for a recompiled port.
3. SKIP (no web access).
4. EE-side (read ../ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h and one generated file ../port/output-usa/sub_0017A0E0_0x17a0e0.cpp): common reasons recompiled R5900 C++ is slow (per-instruction memory-access helpers, cycle counting,
   128-bit register emulation, indirect call dispatch) and known fixes.
Output: concise sections 1-4, then a ranked list of the 5 highest-impact changes for this port with expected
gain. Mark anything you did not confirm in a source as **unverified**.
Use ONLY local files; no web, no shell commands.
