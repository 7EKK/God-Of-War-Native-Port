# Changes to the EE runtime (ps2xRuntime) and recompiler (ps2xRecomp)

All in `ext/PS2Recomp`, uncommitted. "Generic" = correct PS2 behavior for any game.

## Game override (game-specific) — port/overrides/god_of_war_overrides.cpp
Registered for "SCES_531.33", entry 0x100008, CRC32 0xAE7DC7F4. Copied into the runner by sync_runner.sh.
- `GuestReservedLayout`: RPC packet pool 0x80000, RPC server pool 0x90000, TLS pool 0xA0000, async callback stacks
  0xB0000–0xD0000 (8 × 16 KB), runtime allocator 0xD0000–0x100000 (192 KB). Reason: the game heap owns
  0x35E3A8–0x1FF7FF8; upstream put these inside it (allocator after SetupHeap base, pools at 0x1F00000+, callback
  stacks below 0x2000000) → heap corruption.
- `bindAddressHandler(0x27B710, "sceCdReadDvdDualInfo")`, `(0x299928, "sceSifLoadStartModuleBuffer")`
  (the analyzer could not bind them; unbound they call IOP RPC services cdvdfsv/loadfile that the HLE IOP lacks).

## Generic runtime changes
| Change | Files | Why |
|---|---|---|
| `PS2Runtime::GuestReservedLayout` + `setGuestReservedLayout()`, `guestHeapPinned()`; pool helpers read the layout; pinned allocator ignores SetupHeap (returns the game's base) | ps2_runtime.h/.cpp, Syscalls/Helpers/Runtime.h, State.h, System.cpp | let a game relocate runtime-owned guest memory |
| SetupThread fills crt0 `_args` ($a3): argc=1, argv[0]="cdrom0:\\<ELF>;1" | Syscalls/System.cpp | BIOS behavior; games test argv[0] for disc vs host boot |
| `sceCdReadDvdDualInfo(int*, uint*)` handler (layer1 start from DiscSource) | Stubs/CD.cpp, CD.h, ps2_call_list.h | missing; real libcdvd spun on absent cdvdfsv |
| `sceSifLoadStartModuleBuffer` (writes start() result to *a3); buffer load shared with sceSifLoadModuleBuffer | Syscalls/System.cpp/.h, ps2_call_list.h | missing; used by sceChk01 |
| SifLoadModule logs failures | Syscalls/RPC.cpp | failures were silent |
| fioOpen logs failures; `PS2X_TRACE_FILEIO` | Syscalls/FileIO.cpp | diagnostics |
| COP0 Count: recompiler emits `runtime->readCop0Count()`/`writeCop0Count()`; runtime returns low 32 bits of the EE scheduler cycle counter + bias; `cop0_count` field removed from R5900Context; `EeScheduler::currentCycle()` | ps2xRecomp cop0_translator.cpp, ps2_runtime.h/.cpp, ee_scheduler.h, ps2_debug_panel.cpp | Count never advanced → time-based loops never finished |
| GS interrupts: `GS::setInterruptCallback`; SIGNAL (IMR bit 8) / FINISH (IMR bit 9) call it when unmasked; runtime posts `EeEventType::GsInterrupt` → `dispatchIrq(false, 0)` (INTC cause 0) | gs_frontend.h/.cpp, ee_scheduler.h, EeScheduler.cpp, ps2_runtime.cpp | GS interrupt handlers never ran |
| `PS2IopHostAdapter::readPad(port, state)` → padBackend().readState for port 0 only | ps2_iop_host.h/.cpp | feeds the IOP SIO2 DualShock 2 (working) |
| ps2_pad.cpp: keyboard read even when a gamepad is attached (was gamepad XOR keyboard) | src/lib/ps2_pad.cpp | keyboard input was silently ignored on PCs with a controller |
| main.cpp: argv[2] = disc image; heartbeat; live context sampling; peek EE/IOP RAM; PS2X_TRACE_CALLS | src/main.cpp | diagnostics (see diagnostics.md) |
| Switch changes (guarded) | see switch.md | |

## Runtime facts learned
- Uncached RAM mirror 0x20000000–0x3FFFFFFF is mapped to RAM (ps2_address.h / ps2_memory.cpp).
- Upstream reserved regions: kGuestHeapHardLimit 0x1F00000, RPC pools 0x1F00000/0x1F10000, TLS 0x1F20000.
- `EeEventType::Dmac` case does nothing; DMAC handlers are dispatched elsewhere via
  `dispatchDmacHandlersForCause` (Syscalls/Interrupt.cpp).
- GS CSR SIGNAL/FINISH bits are set in gs_frontend.cpp writeRegister (SIGNAL 0x60, FINISH 0x61); CSR writes are W1C.
- Runner sources: `ps2xRuntime/src/runner/*.cpp` (glob), unity batches of 32, PCH includes generated headers.
- Pad backend (ps2_pad.cpp) returns a libpad-style 32-byte block from raylib keyboard/gamepad.

## Graphics pipeline changes (2026-10-09)
| Change | Where | Why |
|---|---|---|
| PCSX2 VU1 interpreter (VUops.cpp, VUflags.cpp, VU1microInterp.cpp copied from PCSX2 v2.8.2, GPL-3.0+) + shims (Common.h, Vif.h, R5900.h, VUmicro.h, GS.h, Gif_Unit.h, MTVU.h) + bridge `pcsx2vu1::run` | src/lib/vu_pcsx2/, ps2_runtime.cpp VU1 MSCAL/MSCNT callbacks | accuracy; default, `PS2X_VU1_LEGACY=1` restores VU1Interpreter. The bridge keeps PCSX2's VU1 regs persistent, sets TOP/ITOP, D/T enables, runs vu1Exec until E-bit (65536-cycle budget) and forwards each finished XGKICK packet to `submitGifPacket(Path1)` |
| Old VU1Interpreter: FLG=3 tags = IMAGE (no abort); optional `PS2X_VU1_INSTANT_XGKICK` | src/lib/vu/ps2_vu1_core.cpp | legacy path only |
| GS: FLG=3 = IMAGE, PRE only for PACKED | src/lib/gs/gs_frontend.cpp | PCSX2 GSState::Transfer |
| VIF1 UNPACK rules (V2/V3/V4-5/MODE3) + truncated-UNPACK warning | src/lib/ps2_vif1_interpreter.cpp | PCSX2 Vif_Unpack.cpp |
| GS dump recorder (`PS2X_GS_RECORD`, `_TEMPLATE`, `_START`, `_FRAMES`) | src/lib/gs/gs_dump_recorder.cpp, include/runtime/gs/gs_dump_recorder.h; hooks in GS::processGIFPacket / processNativePackedGIFPacket / uploadImageNative / writeRegister and EeScheduler VBlankStart | replay our GIF stream in PCSX2 |
| Heartbeat dumps: `PS2X_GS_DUMP` (4 MB VRAM), `PS2X_VU1_DUMP` (micro+data mem), `PS2X_FRAME_DUMP` (presented frame, PPM) | src/main.cpp | diagnostics |
