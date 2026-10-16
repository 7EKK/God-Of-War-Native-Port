# Task: LIBSD transfer + init details (read-only analysis of disassembly)

Use read_file only (no shell commands). Instruction addresses for every claim; mark guesses UNVERIFIED.
Read: results/libsd-api.md (your earlier map), requests/dis/LIBSD.txt, requests/dis/989NOMID.txt.

We replace LIBSD's exports with native C++ (no SPU2 registers, no IOP DMA controller). Exact semantics needed:

1. sceSdVoiceTrans (0x18a0): return value ($v0) for mode 0 (write) and mode 8; what it writes to sound RAM for mode 8
   (used by sceSdSetEffectWorkArea to clear the reverb area); does it return before the transfer completes (async),
   and what state sceSdVoiceTransStatus (0x1af0) checks (flag 0 / flag 1) - so a native version that completes the copy
   immediately can return the same values.
2. sceSdBlockTrans (0x1c30) with mode 0x13 (WRITE | SD_BLOCK_LOOP) as 989snd calls it at 989:0xb930 (find the argument
   values: channel, iop buffer, size, start address), mode 0 at 0xb63c (size 0x400) and mode 2 (STOP) at 0xb7d8:
   - what the SPU2 core does with the data (core input / "ADMA": 512-byte L then R blocks? at which sound-RAM address?)
   - how LIBSD loops over the IOP ring (halves?), and exactly when the registered transfer handler (sceSdSetTransIntrHandler,
     989 handler 0x78bc) is called and with which arguments in loop mode (every block? every half? which channel value?)
   - what 989snd's handler / stream code does with that (which variables it updates, e.g. ring read position), and how
     BVOL (param 0x0F/0x10, set at 989:0xbef0/0xbf10) relates (stream volume).
   - what this stream is used for in 989snd (music? FMV audio? streamed voice?) if visible from the code.
3. sceSdInit (0x456c): every SPU2 register value it writes on init (so a native engine starts in the same state:
   master volumes, VMIX masks, ATTR, reverb off, ...), and which interrupt handlers it registers.
4. sceSdSetEffectWorkArea (0x0984) and sceSdSetEffectMode (0x0c3c): which registers/values they set (ESA/EEA, reverb
   coefficient table, EVOL), so the native engine can store them (reverb mixing may come later).
Output under 160 lines.
