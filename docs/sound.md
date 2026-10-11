# Sound: the native sound engine

The game's sound engine is its own IOP code: 989snd (`989NOMID.IRX`, sequencer, voice allocation, streaming) with the
SMPD streamer plugin, compiled ahead of time like every IRX (docs/native-port.md). Below it there is no sound hardware
model: the SPU2 register-level model and the IOP's SPU DMA channels were removed on 2026-10-11.

## Boundary: the LIBSD API
- 989snd reaches the sound unit **only** through LIBSD.IRX's exports (it never touches SPU2 registers; only LIBSD did).
- Those exports are native C++ (`ps2xIOP/src/native/iop_native_overrides.cpp`, listed in tools/iop/native_overrides.txt):
  sceSdInit #4 0x456c, SetParam #5 0x3670, GetParam #6 0x36cc, SetSwitch #7 0x372c, GetSwitch #8 0x3764, SetAddr #9
  0x379c, GetAddr #10 0x3800, SetCoreAttr #11 0x3cbc, VoiceTrans #17 0x18a0, BlockTrans #18 0x1c30, VoiceTransStatus
  #19 0x1af0, BlockTransStatus #20 0x1e4c, SetEffectAttr #23 0x0c3c, SetTransIntrHandler #26 0x3e3c,
  SetEffectWorkArea #31 0x0984 (addresses checked against LIBSD's export table). Note2Pitch #13 stays game code (math).
- Entry codes as libsd: `(id << 8) | (voice << 1) | core`, master/effect/stream volumes with bit 7 (LIBSD's register
  table at 0x4930 gives the id order: 0 VOLL, 1 VOLR, 2 PITCH, 3 ADSR1, 4 ADSR2, 5 ENVX, 6/7 VOLX, 8 MMIX, 9/A MVOL,
  B/C EVOL, D/E AVOL, F/10 BVOL, 11/12 MVOLX, 13 PMON, 14 NON, 15 KON, 16 KOFF, 17 ENDX, 18-1B VMIX L/EL/R/ER,
  1C ESA, 1D EEA, 1E TSA, 1F IRQA, 20 SSA, 21 LSA, 22 NAX). Addresses are byte addresses (16-byte aligned).

## SoundEngine (`ps2xIOP/src/emulator/core/sound_engine.{h,cpp}`)
- Typed state: per voice volume L/R, pitch, ADSR1/2, start/loop/next addresses, envelope; per core VMIX/NON/PMON masks,
  MMIX, ATTR, master/effect/aux/stream volumes, ESA/EEA. 2 MB sample RAM (the game's sample data).
- DSP (unchanged from the former model, PCSX2 / psx-spx algorithms): ADPCM decode, pitch with linear interpolation,
  ADSR, dry mix per voice, master volume; NAX reported inside the playing block like PCSX2's NextA (989snd's polled
  streams abort when NAX leaves the ring halves: that was the 2026-10-09 music-restart bug).
- Core input stream (BlockTrans, "ADMA"): 1 KB blocks of 256 left + 256 right 16-bit samples read from the IOP ring at
  48 kHz, mixed at BVOL while MMIX's sound-input bits (0xC0) are set; mode 0x13 loops, mode 2 stops. 989snd clears its
  transfer handler before starting it and polls the position. (Not implemented in the former model.)
- Transfers are copies; VoiceTrans returns `(size + 63) & ~63` and, for mode 0, schedules the driver's completion
  handler `handler(channel, arg)` (989snd 0x78bc: releases its transfer lock and wakes the waiting thread) 512 IOP cycles
  later through `IopNativeServices::callGuestLater`, as the DMA interrupt did. VoiceTransStatus returns 1.
- One stereo sample per 768 IOP cycles (`advanceTo`), drained by the host audio thread (`drainOutput`).
- Not mixed yet: reverb (parameters stored), volume sweeps (target applied at once), gaussian interpolation.

## Verified (2026-10-11)
Desert zone with `GOW_AUDIO_PCM`: 158.9 s of audio in a 160 s run (former model 156.5 s), same level profile, same 2
short silences (port/audio_native1 vs audio_native2); zone ok at 60 fps; save test ok.
