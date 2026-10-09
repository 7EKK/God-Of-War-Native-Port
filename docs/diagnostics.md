# Diagnostics

All are opt-in environment variables (added by us; implementations noted).

| Variable | Output | Where implemented |
|---|---|---|
| `PS2X_HEARTBEAT=<s>` | every s seconds: debug PC/RA, EE cycle, **live** context PC + regs (v0 v1 a0-a3 s0-s3 ra), EE threads (status/wait), IOP cycles/modules/threads/RPC servers/services/diagnostics | ps2xRuntime/src/main.cpp |
| `PS2X_HEARTBEAT_PEEK=[iop:]addr:len,…` | hex dump of EE RAM (or IOP RAM with `iop:`) each heartbeat | main.cpp |
| `PS2X_TRACE_CALLS=hex,…` | wraps listed EE functions: `[call] addr(a0..a3) ra=` / `[ret ] addr -> v0=` | main.cpp (replaceFunction; original found via ctx->pc) |
| `PS2X_TRACE_RPC=1` | each RPC served by an IRX: sid, fn, mode, 11 send words, 4 reply words; plus `[sifdma]` IOP→EE DMA (src, dst, size, 4 words) | ps2xIOP iop_rpc.cpp |
| `PS2X_TRACE_IOP_IMPORTS=1` | every IOP import call `lib:ordinal a0 a1 a2 ra` (very verbose, ~400k lines/30 s) | ps2xIOP iop_emulator.cpp dispatchImport |
| `PS2X_TRACE_CDVD=1` | IOP cdvdman calls (`ord=6` = sceCdRead lsn/count/dst) | ps2xIOP iop_cdvd.cpp |
| `PS2X_TRACE_FILEIO=1` | EE fioOpen/fioRead (failed opens always logged) | ps2xRuntime Syscalls/FileIO.cpp |
| `PS2X_TRACE_SIO2=1` | per SEND3 entry: CTRL, FIFO-out fill, port, len, connected, command, fifo/dma, reply, RECV1; DMA ch11/12 setups | ps2xIOP iop_sio2.cpp |
| `PS2X_TRACE_EVF=1` | IOP event flags: set/clear/wait(now/block)/wake with bits and thread id ("thread -1" = no current thread) | ps2xIOP iop_kernel.cpp |
| `PS2X_IOP_WATCH=hex[,hex]` | every IOP guest store to those words: pc, opcode, address, value, ra | ps2xIOP iop_cpu.cpp |

Always-on logs we added: `[SIF module] load-failed/load-unhandled` (RPC.cpp), `fioOpen failed:`.

## Notes
- `m_debugPc` (heartbeat "pc=") only updates at dispatch points and can be stale; the `live pc=` line samples the
  running context (generated code writes ctx->pc per instruction) and is reliable. Sample several times.
- IOP `[import]` lines go through the host log (stdout) while EE `[call]` lines go to stderr; their relative
  order in the log file is not reliable. `[rpc]` lines are printed **after** the server function returns, so the
  imports printed just before belong to that request.
- IOP printf now formats arguments (before: raw "%d"). Old logs (before 2026-10-09) show raw formats.

## Methods that worked
- **Find a hang**: heartbeat live pc → `tools/dis.py` → identify loop → find who sets the awaited flag
  (`grep -l -- '-0xNNNN(' port/output/*.cpp` on the gp/lui-relative offset).
- **Backtrace**: peek the stack (`PS2X_HEARTBEAT_PEEK=1FFFD00:300`), keep words in .text whose word−8 is a `jal`
  (return addresses).
- **Global value**: peek it; initial value: read the ELF at vaddr − 0xFF000.
- **IOP module internals**: `tools/irxdis.py`, import tables (magic 0x41E00000, name at +12, stubs `jr ra; ordinal`),
  call-site arguments in the import trace.
- **Wrong data?** compare peeked EE buffers against the extracted PAK bytes (TOC gives sector offsets).

## PCSX2 as reference (2026-10-09)
- `ref/pcsx2/` portable (portable.txt, inis/PCSX2.ini: BIOS SCPH-70004 v12 PAL, PINE on slot 28011, file log in
  logs/emulog.txt). Boot: `ref/pcsx2/pcsx2-qt.exe -portable -batch -- "<iso>"`. Retail game runs at full speed.
- `tools/pine.py ADDR LEN …` reads EE memory live (`--status`). PINE cannot read IOP RAM.
- IOP RAM / registers / GS state: save a state (PINE opcode 9, slot 1 → `ref/pcsx2/sstates/*.p2s`, a zip with
  iopMemory.bin, eeMemory.bin, GS.bin, …) and extract with 7-Zip. `tools/irxfind.py iopMemory.bin X.IRX` finds where
  PCSX2 loaded each module, so module-relative offsets can be compared with ours.
- Method that found the controller bug: compare the same EE buffer (PINE) and the same IRX-relative IOP structures
  (save state) between PCSX2 and our port, then watchpoint (`PS2X_IOP_WATCH`) the field that differs.

## Graphics diagnostics (2026-10-09)
- `PS2X_GS_RECORD=<out.gs> PS2X_GS_RECORD_TEMPLATE=port/pcsx2state/GS.bin PS2X_GS_RECORD_START=<vsync>
  PS2X_GS_RECORD_FRAMES=<n>` → PCSX2 dump; open with `ref/pcsx2/pcsx2-qt.exe -portable -batch -- <out.gs>`.
  The template is GS.bin from a PCSX2 save state (VRAM at offset 428 is replaced by ours). VSync rate here is
  ~4.5/s at boot; the title is around vsync 190 (~80 s). Colours in the replay are off (CLUT state), geometry is
  faithful.
- `PS2X_FRAME_DUMP=<file.ppm>` (+ `python -I tools/ppm2png.py in.ppm out.png`): the runtime's own frame; use this
  instead of window screenshots when the display may be asleep.
- `PS2X_VU1_DUMP`, `PS2X_GS_DUMP` + `tools/vramdiff.py` for VRAM page maps against PCSX2's GS.bin.
- `PS2X_EE_WATCH=<hex addr>[:<hex len>]` (Windows): logs `[eewatch] write ee <addr> pc=<guest pc> ra=<ra>` for every
  store into that EE RAM range (page guard + single step; ee_watch.cpp). Found the bone-matrix writer this way.
- `PS2X_EE_DUMP=<file>`: 32 MB EE RAM at every heartbeat; compare with eeMemory.bin from a PCSX2 save state
  (PINE opcode 9 saves to a slot) — same ELF, so static/heap addresses usually line up.
- `PS2X_GS_PLAY=<pcsx2 dump.gs>` (+ `PS2X_GS_PLAY_OUT`, `PS2X_GS_PLAY_VRAM`): replay a real-game PCSX2 GS dump
  through our GS; `tools/gsfb.py VRAM FBP FBW W H out.png` decodes a framebuffer from VRAM;
  `tools/gsdump_objects.py a.gs b.gs` compares per-texture vertex extents of two dumps.
- `PS2X_VU1_TRACE_BAD=<file>`: VU1 state at the start of the first microprogram whose kicked packet has saturated
  (0 / 0xFFFx) drawn vertices.

## EE differential tester (2026-10-09) — the most useful tool so far
- PCSX2's R5900 interpreter (R5900OpcodeImpl.cpp, MMI.cpp, FPU.cpp, R5900OpcodeTables.cpp, branch code from
  Interpreter.cpp; GPL-3.0+) lives in ps2xRuntime/src/lib/ee_ref with shims; VU0 macro ops use the embedded PCSX2
  VUops (vu_pcsx2/pcsx2_vu0_cop2.cpp). Syscalls, COP0 and I/O stop a reference run ("not comparable").
- `PS2X_EE_DIFF=<hex>[,...]` (first PS2X_EE_DIFF_MAX calls, default 3) or `PS2X_EE_DIFF=all` (every function's
  first call; only mismatches + a tally are printed). Each call: snapshot CPU + 32 MB RAM + scratchpad, run our
  recompiled function, re-run from the snapshot on the interpreter until it returns to $ra with the entry $sp,
  compare GPRs (128-bit), FPRs, VF regs and every word the reference wrote. `PS2X_EE_DIFF_STEPS` limits steps.
- Expected mismatches: functions replaced by runtime stubs (gow.toml `stubs`: rand, printf/fs/sif/mpeg …) and
  their callers; upper 64 bits of GPRs in a few places. Anything else is a recompiler bug.
