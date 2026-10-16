# LIBSD.IRX API Analysis (989NOMID.IRX Sound Driver)

## 1. Export Address & Identity Table

| Ordinal | SCE Name | LIBSD Address | 989NOMID Stub | Semantics |
|---|---|---|---|---|
| 4 | `sceSdInit` | [`LIBSD:0x456c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4445) | [`989:0x1d190`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29797) | Cold/warm SPU2 init, clears registers, registers DMA & IRQ handlers |
| 5 | `sceSdSetParam` | [`LIBSD:0x3670`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3500) | [`989:0x1d194`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29799) | Sets 16-bit voice or core volume/pitch/ADSR/MMIX parameter |
| 6 | `sceSdGetParam` | [`LIBSD:0x36cc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3523) | [`989:0x1d19c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29801) | Reads 16-bit voice or core parameter (ENVX, MMIX) |
| 7 | `sceSdSetSwitch` | [`LIBSD:0x372c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3533) | [`989:0x1d1a4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29803) | Sets 32-bit voice bitmask (voices 0..23) to switch registers (KON, KOFF, VMIX...) |
| 8 | `sceSdGetSwitch` | [`LIBSD:0x3764`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3547) | [`989:0x1d1ac`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29805) | Reads 32-bit voice bitmask from switch registers |
| 9 | `sceSdSetAddr` | [`LIBSD:0x379c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3561) | [`989:0x1d1b4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29807) | Converts 24-bit byte address to 16-bit halfword offset, writes SSA/LSA/ESA |
| 10 | `sceSdGetAddr` | [`LIBSD:0x3800`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3586) | [`989:0x1d1bc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29809) | Reads SPU2 address register pair, reconstructs 24-bit byte address (NAX) |
| 11 | `sceSdSetCoreAttr` | [`LIBSD:0x3cbc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3889) | [`989:0x1d1c4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29811) | Sets SPU2 core global attributes (ATTR register 0x19A: reverb enable, noise clock) |
| 13 | `sceSdNote2Pitch` | [`LIBSD:0x38a0`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3626) | [`989:0x1d1cc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29813) | Converts center note/fine + note/fine to SPU2 pitch value (4096 = 1.0) |
| 17 | `sceSdVoiceTrans` | [`LIBSD:0x18a0`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1570) | [`989:0x1d1d4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29815) | Starts DMA transfer (IOP <-> sound RAM) on SPU DMA channel 0 or 1 |
| 18 | `sceSdBlockTrans` | [`LIBSD:0x1c30`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1806) | [`989:0x1d1dc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29817) | Starts block transfer or auto-DMA continuous loop transfer (`SD_BLOCK_LOOP`) |
| 19 | `sceSdVoiceTransStatus` | [`LIBSD:0x1af0`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1726) | [`989:0x1d1e4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29819) | Polls (flag 0) or blocks on event flag (flag 1) until `VoiceTrans` finishes |
| 20 | `sceSdBlockTransStatus` | [`LIBSD:0x1e4c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1941) | [`989:0x1d1ec`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29821) | Reads SPU DMA progress: `(loop_count << 24) \| (dma_iop_addr & 0xFFFFFF)` |
| 23 | `sceSdSetEffectMode` | [`LIBSD:0x0c3c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L785) | [`989:0x1d1f4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29823) | Configures reverb effect mode, depth L/R, delay, feedback via `sceSdEffectAttr*` |
| 26 | `sceSdSetTransIntrHandler` | [`LIBSD:0x3e3c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3985) | [`989:0x1d1fc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29825) | Registers DMA transfer completion callback for channel 0 or 1 |
| 31 | `sceSdSetEffectWorkArea` | [`LIBSD:0x0984`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L611) | [`989:0x1d204`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L29827) | Allocates and clears reverb work area in sound RAM (ESA/EEA) |

---

## 2. Argument Decoding & SPU2 Register Mapping

All parameter, switch, and address functions decode their target register through the base table at [`LIBSD:0x4930`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4642):
`reg_base = table[(entry >> 6) & 0x3fc]`. Core stride is `+ core * 0x400`, except MVOL/EVOL/BVOL (bit 7 of `entry` set) where stride is `+ core * 0x28`.

### A. SetParam (#5) & GetParam (#6)
- **Decoding**: `entry = (param_id << 8) | (voice << 1) | core`.
- Voice register stride: `reg_base + core * 0x400 + voice * 0x10`.
- Master/Effect volume stride: `reg_base + core * 0x28` (bit 7 of `entry` set = `0x80`).

| Param ID | Entry Bits | SPU2 HW Register | Name | 989snd Call Sites & Values |
|---|---|---|---|---|
| `0x00` | `0x000 \| (v<<1) \| c` | `0xBF900000 + v*0x10 + c*0x400` | `VOLL` | `989:0x17bf8`, `0x17d68`, `0x1c7e0`, `0xbb44`, `0xbb84`, `0xbc28`, `0xbd14`, `0xbd6c` ($a1 = 0..0x3fff) |
| `0x01` | `0x0100 \| (v<<1) \| c` | `0xBF900002 + v*0x10 + c*0x400` | `VOLR` | `989:0x17c44`, `0x17dbc`, `0x1c800`, `0xbb54`, `0xbbb0`, `0xbc08`, `0xbd40`, `0xbd98` ($a1 = 0..0x3fff) |
| `0x02` | `0x0200 \| (v<<1) \| c` | `0xBF900004 + v*0x10 + c*0x400` | `PITCH` | `989:0x17cc0`, `0x1966c`, `0xba98`, `0xbc70` ($a1 = 4.12 pitch, 0x1000 = 1.0) |
| `0x03` | `0x0300 \| (v<<1) \| c` | `0xBF900006 + v*0x10 + c*0x400` | `ADSR1` | `989:0x17d04`, `0x1a674`, `0xba60`, `0xbc5c` ($a1 = 0x88ff, 0x000f, bank presets) |
| `0x04` | `0x0400 \| (v<<1) \| c` | `0xBF900008 + v*0x10 + c*0x400` | `ADSR2` | `989:0x17d14`, `0x1a684`, `0xba70`, `0xbc6c` ($a1 = 0x9fc6, 0x1fc0, bank presets) |
| `0x05` | `0x0500 \| (v<<1) \| c` | `0xBF90000A + v*0x10 + c*0x400` | `ENVX` | `989:0x10aac` (`GetParam`, polls envelope down to 0) |
| `0x08` | `0x0800 \| c` | `0xBF900198 + c*0x400` | `MMIX` | `989:0xb7b4` (`Get`), `0xb7c4` (`Set`: $a1 &= 0xff3f), `0xb9b8` (`Get`), `0xb9cc` (`Set`: $a1 \|= 0x00c0) |
| `0x09` | `0x0980 \| c` | `0xBF900760 + c*0x28` | `MVOLL` | `989:0x7394` (c0), `0x1baf4` (c1) ($a1 = 0x3fff) |
| `0x0A` | `0x0a80 \| c` | `0xBF900762 + c*0x28` | `MVOLR` | `989:0x73a0` (c0), `0x1bb00` (c1) ($a1 = 0x3fff) |
| `0x0B` | `0x0b80 \| c` | `0xBF900764 + c*0x28` | `EVOLL` | `989:0x73ac`, `0xe2a4`, `0xe37c` (c0: $a1 = 0), `0xe44c` (c1: $a1 = 0) |
| `0x0C` | `0x0c80 \| c` | `0xBF900766 + c*0x28` | `EVOLR` | `989:0x73b8`, `0xe2b4`, `0xe38c` (c0: $a1 = 0), `0xe45c` (c1: $a1 = 0) |
| `0x0F` | `0x0f80 \| c` | `0xBF90076C + c*0x28` | `BVOLL` | `989:0xb798` ($a1 = 0), `0xb99c`, `0xbef0` ($a1 = stream volume L) |
| `0x10` | `0x1080 \| c` | `0xBF90076E + c*0x28` | `BVOLR` | `989:0xb7a8` ($a1 = 0), `0xb9ac`, `0xbf10` ($a1 = stream volume R) |

### B. SetSwitch (#7) & GetSwitch (#8)
- **Decoding**: `entry = (switch_id << 8) | core`.
- 32-bit bitmask splits into: low 16 bits -> `reg_base` (voices 0..15); high 8 bits -> `reg_base + 2` (voices 16..23).

| Switch ID | Entry | SPU2 HW Register | Name | 989snd Call Sites & Usage |
|---|---|---|---|---|
| `0x14` | `0x1400 \| c` | `0xBF900184 + c*0x400` | `NON` | `989:0x1a624` (Get), `0x1a634` (Set: noise enable bitmask) |
| `0x15` | `0x1500 \| c` | `0xBF9001A0 + c*0x400` | `KON` | `989:0x1a8f8` (core 0), `0x1a904` (core 1) (Key On trigger bitmask) |
| `0x16` | `0x1600 \| c` | `0xBF9001A4 + c*0x400` | `KOFF` | `989:0x1a9d4` (core 0), `0x1a9e0` (core 1) (Key Off trigger bitmask) |
| `0x18` | `0x1800 \| c` | `0xBF900188 + c*0x400` | `VMIXL` | `989:0x17c54` (Get), `0x17c6c` (Set), `0xbadc`, `0xbcdc` (dry mix L bitmask) |
| `0x19` | `0x1900 \| c` | `0xBF90018C + c*0x400` | `VMIXEL`| `989:0x1a8a4` (Get), `0x1a8c4`, `0x1a8e0` (Set: effect mix L bitmask) |
| `0x1A` | `0x1a00 \| c` | `0xBF900190 + c*0x400` | `VMIXR` | `989:0x17c7c` (Get), `0x17c88` (Set), `0xbaf8`, `0xbcf8` (dry mix R bitmask) |
| `0x1B` | `0x1b00 \| c` | `0xBF900194 + c*0x400` | `VMIXER`| `989:0x1a8b0` (Get), `0x1a8d0`, `0x1a8ec` (Set: effect mix R bitmask) |

### C. SetAddr (#9) & GetAddr (#10)
- **Decoding**: `entry = (addr_id << 8) | (voice << 1) | core`.
- Per-voice register stride: `reg_base + core * 0x400 + voice * 0xC`.
- Byte address <-> SPU2 halfwords:
  - `SetAddr`: `*(reg) = byte_addr >> 17` (high); `*(reg + 2) = (byte_addr >> 1) & 0xfff8` (low, 16-byte aligned).
  - `GetAddr`: `byte_addr = (high << 17) | (low << 1)`.

| Addr ID | Entry | SPU2 HW Register | Name | 989snd Call Sites & Usage |
|---|---|---|---|---|
| `0x1D` | `0x1d00 \| c` | `0xBF9002E0 + c*0x400` | `ESA` | `989:0x0e10c` ($a1 = sound RAM reverb start byte addr) |
| `0x20` | `0x2040 \| (v<<1) \| c` | `0xBF9001C0 + v*0xC + c*0x400` | `SSA` | `989:0x17cf4`, `0x17e78`, `0xba7c`, `0xbc80` ($a1 = sample start byte addr) |
| `0x21` | `0x2140 \| (v<<1) \| c` | `0xBF9001C4 + v*0xC + c*0x400` | `LSA` | `989:0x13470`, `0x17744`, `0x17cd0`, `0xba88`, `0xbc8c` ($a1 = loop start byte addr) |
| `0x22` | `0x2240 \| (v<<1) \| c` | `0xBF9001C8 + v*0xC + c*0x400` | `NAX` | `989:0x0b588`, `0x132f4`, `0x13300`, `0x1330c` (`GetAddr`, polled 3x; progress = NAX - base) |

### D. SetCoreAttr (#11), SetEffectMode (#23) & SetEffectWorkArea (#31)
- `sceSdSetCoreAttr(entry, value)`: `entry = attr_id | core`.
  - `attr_id = 0x02` / `0x03`: `989:0x7414`, `0x7420`, `0xe398`, `0xe468` ($a1 = 1) -> SPU2 ATTR reverb/DMA mode.
  - `attr_id = 0x08`: `989:0x1a644` ($a1 = noise_clock & 0x3f) -> SPU2 ATTR bits 8..13.
  - `attr_id = 0x0A`: `989:0x7408` ($a1 = 0) -> SPU2 reverb enable/disable.
- `sceSdSetEffectMode(core, sceSdEffectAttr *attr)` ([`LIBSD:0x0c3c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L785), `989:0x0e160`):
  - Struct at `$sp + 0x18`: `core` ($s2 = 0 or 1), `mode` ($s4 = 0..9), `depth_l = 0`, `depth_r = 0`, `delay` ($s5), `feedback` ($s6).
- `sceSdSetEffectWorkArea(core, channel, mode)` ([`LIBSD:0x0984`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L611), `989:0x0e32c`, `0x0e3fc`):
  - Allocates ESA/EEA and clears sound RAM reverb work area using `VoiceTrans` mode 8.

---

## 3. DMA Transfers & Interrupt Handling

### Transfers (#17 VoiceTrans, #18 BlockTrans, #19 Status, #20 Status)
| Ordinal | Function | Mode / Arg Values at 989snd Call Sites | Sound RAM & DMA Handling |
|---|---|---|---|
| #17 | `sceSdVoiceTrans` | `989:0x9d2c`, `0xb4ec`: `channel = 0 \| 1`, `mode = 0` (`SD_TRANS_MODE_WRITE`), `$a2 = iop_addr`, `$a3 = spu_byte_addr`, stack `0x10($sp) = size` | One-shot DMA transfer from IOP memory to SPU2 sound RAM. Converts byte address to TSA (`spu_byte_addr >> 1`). |
| #18 | `sceSdBlockTrans` | `989:0xb63c`: `mode = 0` (`WRITE`), `size = 0x400`<br>`989:0xb7d8`: `mode = 2` (`STOP`), `size = 0`<br>`989:0xb930`: `mode = 0x13` (`WRITE \| SD_BLOCK_LOOP`), `iop_buf`, `ring_size`, `spu_addr` | Auto-DMA ring buffer mode: mode `0x13` enables continuous SPU DMA block looping (`SD_BLOCK_LOOP` bit 4 = `0x10`, write mode 3). DMA streams sound data continuously in a ring. |
| #19 | `sceSdVoiceTransStatus` | `989:0xb50c`: `flag = 1` (`WAIT`)<br>`989:0xe354`, `0xe424`: `flag = 0` (`CHECK`) | When running: flag 0 returns `0` (busy). When complete: returns `1`. Flag 1 blocks on event flag `thevent#10` (`0x52b0 + ch*4`) until DMA interrupt wakes it, then returns `1`. |
| #20 | `sceSdBlockTransStatus` | Uncalled in 989snd (stub `989:0x1d1ec`) | [`LIBSD:0x2b88`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2788) reads DMA channel base register (`0xBF8010C0` / `0xBF8010D0`). Returns `(loop_count << 24) \| (dma_iop_addr & 0xFFFFFF)`. Returns `0` when stopped. |

### Completion Reporting (#26 SetTransIntrHandler)
- **Registration**: `sceSdSetTransIntrHandler(ch, handler, arg)` ([`LIBSD:0x3e3c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L3985), `989:0x9d10` registers `0x78bc` with `arg = 0`; reset to 0 at `0xae90`, `0xaed0`, `0xb408`).
- **Interrupt Source**: SPU DMA channel 0 interrupt (`intrman#4` IRQ `0x24` = 36) and channel 1 (`0x28` = 40), hooked by `sceSdInit` at [`LIBSD:0x45d4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4476) & [`0x45fc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4481) to dispatcher `LIBSD:0x27c8`.
- **Callback Invocation**: [`LIBSD:0x2b2c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2765) calls `handler(channel, arg)` directly from the IOP DMA interrupt context.
- **Handler Behavior (`989:0x78bc`)**:
  1. Releases DMA channel lock (`0xaf1c`).
  2. Calls registered client completion callback (`0x78fc`).
  3. Signals IOP semaphore `thsemap#6` (`-0x1a94`) to wake waiting sound threads.

---

## 4. SPU2 IRQ / Hardware Interrupt Usage
- **SPU2 IRQA**: **Not used by 989snd**.
  - `989NOMID.IRX` never sets `addr_id = 0x1F` (`IRQA` register `0xBF90019C`) via `sceSdSetAddr`.
  - `989NOMID.IRX` never imports or calls `libsd#22` (`sceSdSetSpu2IntrHandler` at [`LIBSD:0x3e90`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4006)) or `libsd#27` ([`LIBSD:0x3eb0`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4014)).
- **Mechanism**: 989snd relies **exclusively** on DMA completion interrupts (#26) and periodic IOP software polling:
  - NAX polling via `sceSdGetAddr(NAX)` at `989:0x132f4..0x1330c` (debounced 3x) for streaming playback tracking.
  - ENVX polling via `sceSdGetParam(ENVX)` at `989:0x10aac` for note release / voice lifecycle tracking.

---
*Confidence: 100% verified against disassembly in [`LIBSD.txt`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt) and [`989NOMID.txt`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt).*
*Unverified: Whether any stripped debug/dead code in 989NOMID ever touched #13 or #20 (primary runtime paths never call them).*
