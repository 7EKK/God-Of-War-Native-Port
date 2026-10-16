# Task: compare GIF PACKED / A+D register decoding (colour, fog, texture state) between PCSX2 and our GS frontend

Read-only. Do not write code.

Files:
- ours: ../ext/PS2Recomp-kl/ps2xRuntime/src/lib/gs/gs_frontend.cpp — function GS::writeRegisterPacked (around lines 1003-1190)
  and GS::writeRegisterUnlocked cases RGBAQ, XYZF2/XYZF3, FOG, FOGCOL, TEXA, TEX0/TEX2, ALPHA, PRMODE/PRMODECONT (around 1195-1560).
- PCSX2: ../ref/pcsx2-src/pcsx2/GS/GSState.cpp — GIFPackedRegHandler* and GIFRegHandler* functions
  (RGBA, STQ, UV, XYZF2, XYZ2, FOG, A_D, NOP, XYZF3, XYZ3), plus GIFRegHandlerFOGCOL/TEXA/TEX0/TEX2/PRMODE/PRMODECONT.
  Register bit layouts: ../ref/pcsx2-src/pcsx2/GS/GSRegs.h.

Question: list every place where our decoding of a field (bit position, width, which 64-bit half, ordering of
packed STQ vs RGBA Q handling, FOG field in packed XYZF2 and packed FOG, FOGCOL FCR/FCG/FCB, TEXA TA0/AEM/TA1,
PRMODE attribute bits) differs from PCSX2. For each difference give: our file:line, PCSX2 file:line, both
expressions, and whether it could change a pixel's colour. If no difference for an item, say "same" in one line.

Output: a short table. Mark anything you did not verify by reading the code as UNVERIFIED.
