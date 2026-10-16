# Task: the LIBSD.IRX API as the game's 989snd driver uses it (read-only analysis of disassembly)

Use read_file only (no shell commands). Give instruction addresses for every claim; mark guesses UNVERIFIED.

Context: native PC port of God of War (PS2). The game's sound driver 989NOMID.IRX reaches the SPU2 only through LIBSD
(imports libsd#4,5,6,7,8,9,10,11,13,17,18,19,20,23,26,31; #5 SetParam is called from 134 sites). LIBSD is the only module
that touches SPU2 registers (0xBF900000..). We will replace LIBSD's exported functions with native C++ that drives a
native voice mixer (no SPU2 register model). We need their exact semantics.

Read: requests/dis/LIBSD.txt (full disassembly + .rodata/.data; the IRX export table "Sound_Device_Library" lists the
exported function addresses in order: entry 0 at the word after the name block = ordinal 0, ... find ordinals 4..31),
requests/dis/989NOMID.txt (callers; `; -> libsd#N`), ../ext/PS2Recomp-kl/ps2xIOP/src/emulator/core/spu2.h and spu2.cpp
(our current register-level SPU2 model: which registers it implements), ../docs/game-internals.md, ../docs/status.md
(music/NAX notes).

For every ordinal the driver imports (4,5,6,7,8,9,10,11,13,17,18,19,20,23,26,31):
1. Export address in LIBSD and the standard libsd name (sceSdInit, sceSdSetParam, ... confirm by behaviour).
2. Arguments and how they are decoded (e.g. SetParam(entry, value): entry bits = core | voice<<1 | param id; list every
   param id 989snd uses with the SPU2 register it writes: VOLL, VOLR, PITCH, ADSR1, ADSR2, ENVX, VOLXL/R, MMIX..., and
   for SetSwitch: KON/KOFF/PMON/NON/VMIXL/VMIXEL/...; SetAddr: SSA/LSAX/NAX/ESA/EEA/IRQA/TSA...; core attributes;
   effect attributes (reverb mode, depth, delay, feedback)).
3. For the transfers (#17 VoiceTrans, #18 BlockTrans, #19/#20 status, #26 SetTransIntrHandler, #31): the modes used by
   989snd (with argument values found at the call sites), the DMA channel / sound-RAM address handling, the auto-DMA
   ring (block transfer with BLOCK_LOOP), how completion is reported (handler called with which args, from which
   interrupt), and what #19/#20 return while a transfer runs vs. after.
4. Any SPU2 IRQ use (IRQA, interrupt handler registration) by 989snd.
Output: one compact table per function group + call-site argument values; under 200 lines.
