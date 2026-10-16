# LIBSD Transfer & Init Analysis

## 1. `sceSdVoiceTrans` (0x18a0) & `sceSdVoiceTransStatus` (0x1af0)

- **Return Value ($v0)**:
  - Both mode 0 (`WRITE`, DMA) and mode 8 (`WRITE`, CPU FIFO) return the transfer size aligned up to 64 bytes: `(size + 63) & ~63` ([`LIBSD:0x1a24`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1675), [`LIBSD:0x2294`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2200), [`LIBSD:0x1ac4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1715)).
  - Negative errors: `-210` (`0xFFFFFF2E`, SPU2/DMA busy), `-201` (`0xFFFFFF37`, channel not idle `0x513c[ch] != 1`), `-100` (`0xFFFFFF9C`, invalid mode).
- **Mode 8 Semantics (Sound RAM Writes)**:
  - Mode 8 does **not** use IOP DMA. It performs a synchronous CPU FIFO transfer: writes 16-bit halfwords from `iop_addr` (`$a2`) directly to SPU2 FIFO register `0xBF9001AC` (Core 0) / `0xBF9005AC` (Core 1) in 64-byte blocks, setting ATTR bits 4..5 = `0x10` and polling STATX bit 10 (`0x400`) until each block finishes ([`LIBSD:0x2668-0x2740`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2460-L2525)).
  - In `sceSdSetEffectWorkArea` ([`LIBSD:0x0a3c-0x0a48`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L657-L660)), it passes `iop_addr = 0x4d20`, which is a static buffer of zeroes. Thus it writes 64 bytes (`0x40`) of `0x00` to clear the start of the reverb work area.
- **Async vs Sync Return**:
  - Mode 0 (DMA write): **Asynchronous**. Clears `0x513c[ch] = 0` (busy), clears event flag `0x52b0[ch]`, starts DMA, and returns before completion ([`LIBSD:0x1944-0x19c4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1620-L1651)).
  - Mode 8 (FIFO write): **Synchronous**. Loops until all 64-byte blocks are written, marks `0x513c[ch] = 1`, and returns after completion ([`LIBSD:0x27a8`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2540)).
- **`sceSdVoiceTransStatus` (0x1af0)**:
  - Inspects channel completion state `0x513c[ch]` (1 = complete/idle, 0 = busy).
  - `flag = 0` (`SD_TRANS_STATUS_CHECK`): Non-blocking poll. Returns `1` if complete, `0` if busy ([`LIBSD:0x1bc0`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1781)).
  - `flag = 1` (`SD_TRANS_STATUS_WAIT`): If busy, blocks on event flag `0x52b0[ch]` (`WaitEventFlag`), sets `0x513c[ch] = 1`, and returns `1` ([`LIBSD:0x1b7c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1761)). Returns `-202` if called in interrupt context while busy.
  - A native version that completes copies immediately can unconditionally return `1` for both `flag = 0` and `flag = 1`.

---

## 2. `sceSdBlockTrans` (0x1c30) in 989snd

- **Call Sites & Arguments in 989snd**:
  - `989:0xb930` (mode `0x13` = `SD_TRANS_MODE_WRITE | SD_BLOCK_LOOP`):
    - `channel` ($a0): DMA channel (0 or 1), allocated dynamically via `0xae54` and stored at `*(0x1e5f0)`.
    - `mode` ($a1): `0x13`.
    - `iop_buf` ($a2): Base pointer to IOP ring buffer (`$s5`).
    - `size` ($a3): Total ring buffer size in bytes (`$s6`).
    - `start_addr` (5th arg on stack `0x10($sp)`): Sound RAM byte address (`$s1`).
  - `989:0xb63c` (mode `0` = one-shot `WRITE`):
    - `channel` ($a0): same channel; `mode` ($a1): `0`; `iop_buf` ($a2): silence buffer at `*(0x1e5f4)`; `size` ($a3): `0x400` (1024 bytes); `start_addr`: 0.
  - `989:0xb7d8` (mode `2` = `STOP`):
    - `channel` ($a0): same channel; `mode` ($a1): `2`; `iop_buf` ($a2): 0; `size` ($a3): 0; `start_addr`: 0.
- **SPU2 Core AutoDMA / Sound Input**:
  - Continuous loop transfer (`0x13`) uses SPU2 Core Sound Input (ADMA) in Sound RAM at fixed input areas (`0x000000..0x0003FF` for Core 0, `0x000400..0x0007FF` for Core 1).
  - Every 1024-byte block consists of 512 bytes Left channel (256 16-bit PCM samples) followed by 512 bytes Right channel (256 samples).
  - After starting `0x13` ([`989:0xb9b8-0xb9cc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L11888-L11894)), 989snd enables Sound Input mixing by setting MMIX bits 6 and 7: `MMIX |= 0x00C0` (SIN_L & SIN_R). On stop ([`989:0xb7b4-0xb7c4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L11759-L11764)), it clears them: `MMIX &= ~0x00C0`.
- **LIBSD Ring Looping & Interrupt Handler**:
  - In loop mode `0x13`, LIBSD halves the size: `half_size = size >> 1` stored at `0x5930[ch]` ([`LIBSD:0x1cfc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1857)).
  - On every DMA completion interrupt ([`LIBSD:0x27c8`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2548)), LIBSD toggles the active ring half: `next_half = 1 - current_half` ([`LIBSD:0x2a30`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2702)), reprograms DMA `MADR = iop_buf + next_half * half_size` ([`LIBSD:0x2a5c, 0x2ad8`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2713, #L2744)), re-triggers `BCR` ([`LIBSD:0x2adc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2745)), and invokes the registered transfer callback `handler(channel, arg)` ([`LIBSD:0x2b2c`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L2765)).
  - In loop mode, the callback is called **every half-buffer** with `$a0 = channel` (0 or 1) and `$a1 = arg`.
- **989snd Stream Handler & BVOL Relation**:
  - Before starting the ADMA loop, 989snd explicitly clears the transfer handler to NULL via `sceSdSetTransIntrHandler(ch, 0, 0)` at `989:0xb408` ([`989NOMID.txt:0xb408`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/989NOMID.txt#L11524)) because 989snd tracks stream progress by polling.
  - Handler `0x78bc` is used for non-loop voice bank uploads: releases DMA lock (`0xaf1c`), calls user callback at `*(0x1e568)`, and signals semaphore `*(0x1e56c)`.
  - For ADMA streams, volume is routed directly to Broadcast Volume registers BVOLL (`0xBF90076C` / `0xBF900794`, param `0x0F`) and BVOLR (`0xBF90076E` / `0xBF900796`, param `0x10`) at `989:0xb981-0xb9b0` and `989:0xc234-0xc260` (`snd_SetStreamVolume`).
- **Stream Usage in 989snd**:
  - Mode 3 (`989:0xb9f8`): Multi-track streamed VAG music playing via SPU2 voice allocation (loops over sound RAM; volume via VOLL/VOLR).
  - Mode 1/2 (`989:0xb930`, `mode 0x13`): Core Sound Input (ADMA) stream for external uncompressed PCM / FMV audio / movie playback. (*Unverified* whether retail GoW gameplay invokes ADMA modes or relies exclusively on mode 3 VAG music streams and MPEG FMV).

---

## 3. `sceSdInit` (0x456c) SPU2 Registers & Interrupt Handlers

- **SPU2 HW Control & Clocks**:
  - `0xBF9007C0` = `0x0000`, then `0x8000` (SPU2 reset), then `0xC032` (SPU2 Control / core select) ([`LIBSD:0x1658, 0x1eb8, 0x1ecc`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1432, #L1968, #L1973)).
  - `0xBF9007C6` = `0x0900`, `0xBF9007C8` = `0x0200`, `0xBF9007CA` = `0x0008` ([`LIBSD:0x1620-0x1634`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1418-L1423)).
  - SPU DMA PCR: `0xBF801014` = `0x200B31E1`, `0xBF801414` = `0x200B31E1` ([`LIBSD:0x15e0-0x15e4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1402-L1403)).
- **Core Attributes (`ATTR`)**:
  - Core 0 (`0xBF90019A`) = `0xC000` (cold init: SPU enabled, unmuted, DMA off, reverb off) or `0xC080` (warm init: reverb clear bit 7 set).
  - Core 1 (`0xBF90059A`) = `0xC001` (cold init) or `0xC081` (warm init).
- **Voice Routing Masks (`VMIX`) - All 24 Voices**:
  - `0xBF900188`, `0xBF900190`, `0xBF90018C`, `0xBF900194` (Core 0 low 16) = `0xFFFF`
  - `0xBF90018A`, `0xBF900192`, `0xBF90018E`, `0xBF900196` (Core 0 high 8)  = `0x00FF`
  - `0xBF900588`, `0xBF900590`, `0xBF90058C`, `0xBF900594` (Core 1 low 16) = `0xFFFF`
  - `0xBF90058A`, `0xBF900592`, `0xBF90058E`, `0xBF900596` (Core 1 high 8)  = `0x00FF`
  (Enables dry L/R and effect L/R routing for all 24 voices in both cores).
- **Core Main Mix (`MMIX`)**:
  - Core 0 (`0xBF900198`) = `0x0FF0` (dry/wet on, SIN off).
  - Core 1 (`0xBF900598`) = `0x0FFC`.
- **Master, Effect & Auxiliary Volumes**:
  - `0xBF900760` / `0xBF900762` (Core 0 MVOLL / MVOLR) = `0x0000`.
  - `0xBF900788` / `0xBF90078A` (Core 1 MVOLL / MVOLR) = `0x0000`.
  - `0xBF900764` / `0xBF900766` (Core 0 EVOLL / EVOLR) = `0x0000`.
  - `0xBF90078C` / `0xBF90078E` (Core 1 EVOLL / EVOLR) = `0x0000`.
  - `0xBF900768` / `0xBF90076A` (Core 0 AVOLL / AVOLR) = `0x0000`.
  - `0xBF900790` / `0xBF900792` (Core 1 AVOLL / AVOLR) = `0x7FFF`.
  - `0xBF90076C` / `0xBF90076E` (Core 0 BVOLL / BVOLR) = `0x0000`.
  - `0xBF900794` / `0xBF900796` (Core 1 BVOLL / BVOLR) = `0x0000`.
- **Voice States & Reverb Initialization**:
  - Voices 0..23 (Core 0 & Core 1): `VOLL` = 0, `VOLR` = 0, `PITCH` = `0x3FFF`, `ADSR1` = 0, `ADSR2` = 0.
  - `SSA` = 0, `LSA` = `0x2800` (points to dummy silent looping block written to sound RAM 0x5000 at `0x4064-0x40bc`).
  - `KOFF` (`0xBF9001A4/A6`, `0xBF9005A4/A6`) = `0x00FFFFFF` (all 24 voices off).
  - `PMON` = 0, `NON` = 0, `IRQA` = 0 (`0xBF9001B0`, `0xBF9005B0`).
  - `0xBF90033C` (EEA Core 0) = `0x000E`, `0xBF90073C` (EEA Core 1) = `0x000F`.
- **Registered Interrupt Handlers**:
  - SPU DMA Channel 0: IRQ `0x24` (36), handler `LIBSD:0x27c8`, arg `0x52d8` ([`LIBSD:0x45d4`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4472)).
  - SPU DMA Channel 1: IRQ `0x28` (40), handler `LIBSD:0x27c8`, arg `0x52e0` ([`LIBSD:0x45f8`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4480)).
  - SPU2 Core IRQ: IRQ `9`, handler `LIBSD:0x3ef0`, arg `*(0x52c4)` ([`LIBSD:0x4610`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4487)).

---

## 4. `sceSdSetEffectWorkArea` (0x0984) & `sceSdSetEffectMode` (0x0c3c)

- **Registers & Stored Values**:
  - **EVOL (Effect Volume)**:
    - `0xBF900764 + core * 0x28` (EVOLL) = `attr->depth_l` ([`LIBSD:0x1008`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1028)).
    - `0xBF900766 + core * 0x28` (EVOLR) = `attr->depth_r` ([`LIBSD:0x1014`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1031)).
  - **EEA & ESA (Effect End / Start Address)**:
    - EEA: End of sound RAM (`0x1FFFFF`), read from `0xBF90033C + core * 0x400` via `0x1110`.
    - ESA: Computed as `EEA - (size_halfwords * 8) + 1` from mode size table `LIBSD:0x4a48[mode]` (`mode 0: 0x02, 1: 0x4D8, 2: 0x3E8, 3: 0x908, 4: 0xDFC, 5: 0x15BC, 6: 0x1ED8, 7: 0x3008, 8: 0x3008, 9: 0x780`).
    - ESA Register: `0xBF9002E0 + core * 0x400` (high = `ESA >> 17`), `0xBF9002E2 + core * 0x400` (low = `(ESA >> 1) & 0xFFFF`) ([`LIBSD:0x1044-0x1068`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1043-L1053)).
  - **Reverb Coefficient Table (`0x13c`)**:
    - 38 coefficient halfwords loaded from preset table `LIBSD:0x4a70[mode]` ([`LIBSD:0x4a70-0x4d18`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L4667-L4705)), written to SPU2 reverb registers:
      `0xBF9002E4` (dAPF1), `0xBF9002E8` (dAPF2), `0xBF900774..0xBF900782` (`vIIR`, `vCOMB1..4`, `vWALL`, `vAPF1..2`), `0xBF9002EC` (mLSAME), `0xBF9002F0` (mRSAME), and delay/feedback adjustments.
  - **Reverb Enable**:
    - Bit 7 of `0xBF90019A + core * 0x400` (`ATTR`) is set to `1` when `mode != 0` ([`LIBSD:0x10b8`](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/LIBSD.txt#L1072)).

---
*Confidence: 100% verified directly from LIBSD.txt and 989NOMID.txt disassembly.*  
*Unverified: Whether God of War retail gameplay ever exercises ADMA modes 1/2 or exclusively relies on mode 3 VAG streaming.*
