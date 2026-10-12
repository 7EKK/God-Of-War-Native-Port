# FMV (movies)

Status: **not playing yet**. Play and test runs keep `GOW_SKIP_FMV=1`, which replaces the movie API
(0x188CA0 load, 0x188E80/ED8/EF0/F20) so the game continues at once. With it set the game behaves as before.

## What exists
- **Decoder**: the game's libmpeg entry points (`sceMpeg*`, stubbed in the recompiler config) are served by the runtime
  (`ps2xRuntime/src/lib/Kernel/Stubs/MPEG.cpp`). MPEG-2 video is decoded by the vendored **libmpeg2 0.5.1**
  (`ps2xRuntime/third_party/libmpeg2`, GPL-2-or-later, plain C, no CPU-specific code, also builds for the Switch).
  FFmpeg is no longer used (`PS2X_ENABLE_FFMPEG`, default OFF, keeps it as an opt-in backend).
  Verified: the intro movie (`Semo01`) decodes to correct 640x480 pictures with increasing PTS
  (RAM dump of the picture: Kratos' feet on the cliff).
- `sceMpegGetPicture` writes at most `mbcount` macroblocks (16x16 RGB32 = 1024 bytes each), like the original library
  (0x26FE60 stores `rgb32 | 0x20000000` and `mbcount`; `_sceMpegOutputFrame` 0x2705E0 raises its error path when
  `mbcount` is smaller than the picture and writes nothing).
- `GOW_MPEG_DIAG=1` traces the pictures handed to the game.

## How the game plays a movie (USA build)
- Movie thread 0x189020, state in 0x29C838: 3..9 load, **10** = loaded, waiting; **11** = playing. 0x188E80 (start
  request, also sends SMPD command 0x1C) is called by the level script 0x21ED08 when the scene is ready. The Kratos
  frames before state 11 are the in-engine intro, not the movie.
- File extension picks the mode (0x2B2F50 ".m2" -> mode 2, ".ps" -> mode 1). Mode 2 skips the sequence-header probe,
  so the picture size fields at movie struct +0x58/+0x5C stay 0 and the game calls
  `sceMpegGetPicture(mp, buf, mbcount = 0)` with a 0x40-byte buffer at 0x148FBC0; its GS upload packet
  (cnt 3: A+D BITBLTBUF dbp 0x2800 dbw 12 / TRXREG, then ret) is allocated right after it at 0x148FC00.
- The movie's VBlank handler 0x26CF20 calls 0x26E798 (PMODE/SMODE2/DISPFB/DISPLAY stores, field from CSR) and kicks
  that packet (GIF chain, TADR from the queue at 0x30A448) and the draw packet at 0x30A340 (FRAME fbp 104, TEX0 tbp
  0x2800 CT32, sprites). Double-buffered display between fbp 104 and 208.

## Open
- **Blocker**: with `mbcount = 0` the original library writes no RGB data, so the pictures must reach GS memory at
  tbp 0x2800 some other way (the packet sets BITBLTBUF/TRXREG but no TRXPOS/TRXDIR and no IMAGE data). That path is
  not identified yet. Before the `mbcount` bound, the runtime wrote 1.2 MB over the packet and the heap after it
  (the flat-coloured garbage seen during the movie).
- Movie audio (SMPD stream) not checked yet.
- The movie code still stores to IPU / DMA channel 3/4 registers (open 0x189C30.., callback 0x18AAA8 reads ch4
  CHCR); they reach no device (no IPU model exists).
