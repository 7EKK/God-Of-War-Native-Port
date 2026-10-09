# God of War internals (PAL SCES-53133)

Addresses are EE virtual addresses in SCES_531.33 unless prefixed `iop:`. "gp-relative" globals are written as
absolute addresses (e.g. `-0x2BF0($v1)` with `lui v1,0x2A` = 0x29D410).

## Boot flow
- crt0 0x100008: SetupThread syscall 0x3C (gp 0x2D1070, stack 0x1FF8000 size 0x8000, **args buffer $a3 = 0x2C9080**),
  SetupHeap 0x3D (0x35E3A8, 0), _InitSys 0x29A0E0, 0x294AE0, heap init 0x138CB0, static ctors, main 0x138D48.
- Static ctor table 0x2A7F2C: count 2 → 0x26C248 (calls 0x235180, builds tables incl. IRX module table) and 0x29C7B8.
- 0x17AB60(argc, argv): argv[0] starts with "cdrom" → boot flag 0x29D124 = 0 (disc); else 1 (host); argc 0 → stays
  −1 (treated as host). Disc mode: IOP reboot with `cdrom0:\IOPRP300.IMG;1`, modules from `cdrom0:\`, retries on
  failure. Host mode: `IOP_MOD/<name>`, no reboot, no retry.
- IOP init 0x17BD58: sceSifInitRpc, sceCdInit, sceCdMmode(2), reboot IOP (0x299D60/0x299D10), sceCdReadDvdDualInfo
  (0x27B710), sceFsReset, sceSifLoadFileReset, sceSifInitIopHeap, then loads the 8-entry module table at 0x2FF9F0
  (name, args): sio2man, dbcman, sio2d, mc2_d, ds2u_d, libsd, 989nomid (args "rpc_priority=64"), smpd_iop; then
  sceDbcInit 0x27B910, 0x27CD10, sceChk01 0x2841E0.

## Memory
- Game heap (allocator create 0x13D758 from 0x138CB0): all RAM from .bss end 0x35E3A8 to **0x1FF7FF8**.
  Heap stack at 0x2CD9C0; alloc 0x13DCC0 (walks a free list; corrupted list = infinite loop at 0x13DD20).
- Main stack 0x1FF8000–0x2000000. Fixed-block pool "RB NODE Heap" at 0x4558E0 (16-byte nodes, 16-bit indices).
- The game reads IOP-written status through the uncached mirror (e.g. 0x20455640).
- **Runtime-owned guest data must avoid 0x35E3A8–0x2000000** → override moves it into kernel RAM 0x80000–0x100000
  (see runtime-changes.md, GuestReservedLayout).

## Known SDK function addresses (bound or relevant)
sceCdSync 0x27B1B8, sceCdSyncS 0x27B258, _sceCd_scmd_prechk 0x27B2C8, sceCdInit 0x27B420,
sceCdReadDvdDualInfo 0x27B710 (bound by override), sceCdMmode 0x27B7E0, sceDbcInit 0x27B910, scePad2Init 0x27C3F8,
scePad2CreateSocket 0x27C4A0, scePad2Read 0x27C600, scePad2GetState 0x27C7A8, sceSifInitRpc 0x297888,
sceSifBindRpc 0x297ED0, sceSifCallRpc 0x2980B0, sceSifCheckStatRpc 0x2982B0, sceOpen 0x298CD0, sceRead 0x2990E0,
sceSifAllocIopHeap 0x299438, _sceSifLoadModuleBuffer 0x299718, sceSifLoadStartModuleBuffer 0x299928 (bound by
override), _sceSifLoadModule 0x299948, sceSifRebootIop 0x299D60, sceChk01 0x2841E0, sceScfGetLanguage 0x283C98.

## Waits/halts found (each was a hang until fixed)
| Address | What waits | Released by |
|---|---|---|
| 0x13DD20 | heap alloc loop | runtime data out of the game heap |
| 0x27B378 | libcdvd bind loop (IOP RPC 0x80000593) | runtime handler for sceCdReadDvdDualInfo |
| 0x27B978 | libdbc bind loop (RPC 0x80001300) | modules actually loaded (boot argv fix) |
| 0x186080 | deliberate halt if sceChk01 fails | ck01 load path fixed |
| 0x176B58 | RB-tree walk with NULL object 0x29DA60 | resource load succeeding |
| 0x186160–0x186180 | loader pump loop (0x185B10 returns 1) | 989snd batch callbacks (NOP patch removal) |
| 0x183968 | `while (*0x29D410 == 1)` "Immediate Upload" wait | GS FINISH interrupt → handler 0x182EC8 |

## sceChk01 (0x2841E0)
Uploads embedded IRX "ck01" (.data 0x2A5410, 857 bytes, imports cdvdman) with sceSifAllocIopHeap + DMA +
sceSifLoadStartModuleBuffer; ck01's start() calls cdvdman sceCdSC(−9) and returns 5 if it returned 1, else 1.
Game continues only if load ≥ 0 and (result & ~3) == 0; otherwise spins at 0x186080. Results stored at
0x29D11C (ret) / 0x29D120 (result).

## Resource loading (EE ↔ IOP)
- Main game object `*0x337BA8`; loader 0x186018(obj, name, flags 0x10000210): zeroes obj+0x54, starts load
  0x17AE48, pump loop {0x26C4B0, 0x1730A8, 0x1733F0} until 0x185B10 returns 0. 0x1BB6E0 then copies obj->0x54 to
  global 0x29DA60. Resource name from 0x1BB390: "R_Perm" + language letter (byte at 0x29DAA6) → "R_PermU".
- Loader state machine 0x185B10 (object at 0x5CCAE8, state at +0) uses `mfc0 Count` timeouts (needs a running Count).
  Global pause counter 0x29D444 (0x172CE0 increments, 0x172D20 decrements); 0x185B10 returns 1 early while non-zero.
- Sound operations list head 0x2FF948; node state +0x34: 1 = requested, 2 = waiting for bank flag, 3 = registering
  entries (RPC 'SMPD' fn 0x13, 8 per pump). 1→2 by batch callback **0x172D78** (registered via 0x26D670 →
  0x26CCE8 with the request); 2→3 when status entry (slot = node+0x24 & 0xF) at `*0x29CA70 + slot*32 + 0x48` has
  bit 0x2000.

## 989snd / SMPD protocol
- EE 989snd library: RPC client 0x307748 to IOP sid **0x123456** (989snd server).
  - Direct calls 0x26CAC0(cmd, size, buf): sceSifCallRpc NOWAIT, wait 0x26C730, result = reply word 1 (0x307784).
  - Batched calls 0x26CCE8 → flushed by 0x26D050 as cmd **0x4D** (results array count*4+8 bytes); per-command
    callbacks run from 0x26C4B0 when the pending pointer 0x2A30C8 (set at 0x26C808, cleared in 0x26C730) completes.
    Completion = sentinels −1 in result words 0 and count+1.
  - cmd 0x68 = plugin call: payload `'SMPD'(0x534D5044), fn, size, args…`.
- SMPD (game IRX "smpd file streamer", plugin of 989snd; descriptor iop module offset 0x8748: 'SMPD', 0x1E functions):
  - fn 1 init (reads PVD, root dir, GODOFWAR.TOC; builds per-extension tables at iop 0x82A40 WAD, 0x85B68 VAG, …:
    8-char name, size, location; extensions VAG VPK WAD M2V PSS M2W PSW PAK at iop 0x5BB50),
  - fn 0xF open resource (name + flags) → handle, fn 6 fill 128 KB EE buffer (0x532840/0x542840/0x552840/0x562840,
    DMA in 64 KB pieces), fn 0xD load sound bank from EE memory (e.g. "GENERAL" EE 0x1EDE520 size 0x10F1E0) via
    sceSifGetOtherData 8 KB chunks + snd989 #129, fn 0x13 register sound entries.
  - Status block 0x280 bytes DMA'd to EE 0x455640 (word 0 = sequence counter; entries 32 bytes; flags at +0x1C).
- IOP 989snd module args: "rpc_priority=64" (parsed with strncmp chain; unknown → "989snd Error: cause 7").

## GS
- GS interrupt handler 0x182EC8 (INTC cause 0): reads CSR 0x12001000; FINISH bit → sets 0x29D410 = 2.
- Upload helper 0x13F778(n): sceDmaSend(*0x3331A0 or *0x3331A4, *0x33319C), packet ends with FINISH.

## Controller (working since 2026-10-09; see iop-changes.md for the fixes)
- (Before the fix the title showed "PLEASE INSERT AN ANALOG CONTROLLER ... PORT 1".) EE socket buffer for port 1: 0x2FFB00 (+0x1C buttons/sticks), DBCMAN status block EE 0x30F0C0.
- Stack: EE libpad2 (scePad2*) + libdbc (sceDbc*, RPC 0x80001300) → IOP DBCMAN.IRX ↔ DS2U_D.IRX ↔ SIO2D.IRX ↔
  SIO2MAN.IRX → SIO2 hardware. Also MC2_D (memory card) uses SIO2.
- IRX imports: SIO2MAN: intrman[4,5,6,7,17,18] dmacman[28,32,33,34,35] thevent thsemap sysclib;
  SIO2D: sio2man[51,61,62,63] secrman[6] vblank[5]; DS2U_D: sio2man[51,61,62] sio2d[6,9,10] dbcman[4,6,7,8];
  DBCMAN: sifcmd/sifman RPC server; MC2_D: sio2man, sio2d, dbcman, secrman[4,5], cdvdman[24].
- SIO2MAN code (module offsets): register helpers at 0x0–0x188 (0xBF808268 CTRL, 0x826C RECV1, 0x8270 RECV2,
  0x8274 RECV3, 0x8278/0x827C, 0x8280 ISTAT, SEND3 at 0x8200+4i, SEND1/2 at 0x8240+8i, FIFO in sb 0x8260,
  FIFO out lbu 0x8264); transfer 0x1D4: set SEND1/2 (4), SEND3 (16), push in-bytes to FIFO, then
  dmacman#28 dmac_request(ch 11, …)+#32 dmac_transfer(11) and #28(ch 12, …)+#32(12), then CTRL|=1 (0x2FC).
  Init 0x77C: RegisterIntrHandler(0x11, handler 0x584), EnableIntr(0x11), dmacman#35(11,3)/(12,3).
