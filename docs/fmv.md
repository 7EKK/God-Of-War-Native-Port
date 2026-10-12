# FMV (movies)

Status: **the intro movie plays natively and ends; the game then continues to gameplay. Movie audio is still silent.**
Play and zone runs still set `GOW_SKIP_FMV=1` (it replaces the movie API: 0x188CA0 load, 0x188E80/ED8/EF0/F20, so the
game continues at once); without it the movies play.

## What exists
- **Decoder**: the game's libmpeg entry points (`sceMpeg*`, stubbed in the recompiler config) are served by the runtime
  (`ps2xRuntime/src/lib/Kernel/Stubs/MPEG.cpp`). MPEG-2 video is decoded by the vendored **libmpeg2 0.5.1**
  (`ps2xRuntime/third_party/libmpeg2`, GPL-2-or-later, plain C, no CPU-specific code, also builds for the Switch).
  FFmpeg is no longer used (`PS2X_ENABLE_FFMPEG`, default OFF, keeps it as an opt-in backend).
- `sceMpegGetPicture` writes at most `mbcount` macroblocks (16x16 RGB32 = 1024 bytes each), like the original library.
- **Stream callbacks run inside `sceMpegDemuxPss`**, nested on the demuxing thread (`dispatchGuestStreamCallback`), as
  libmpeg does. They were queued for later before; the game's player depends on the synchronous call (below).
  A callback that does not return (thread switch) falls back to the queued invocation and logs `[mpeg] stream callback
  ... did not return`.
- **End of stream**: libmpeg's `sceMpegIsEnd` is the game's own code (0x26FF70: returns the first word of `mp->sys`).
  The stub sets that word when the PSS program end was seen and every decoded picture was handed out, and clears it in
  `sceMpegCreate`. Without it the player stayed in its playing state forever after the last picture.
- GS renderer: `TEST.ZTE = 0` means no depth test (the GS ignores ZTST then). The movie draws its picture sprite with
  `TEST = 0`; it was treated as ZTST NEVER and rejected after the first frame, which froze the picture.
- `GOW_MPEG_DIAG=1`: pictures handed to the game, `AddStrCallback` registrations, stream-callback call counts.

## How the game plays a movie (USA build)
- Movie thread 0x189020, state in 0x29C838: 3..9 load, **10** = loaded, waiting; **11** = playing. 0x188E80 (start
  request, also sends SMPD command 0x1C) is called by the level script 0x21ED08 when the scene is ready. The Kratos
  frames before state 11 are the in-engine intro, not the movie.
- Open (0x189BD0, movie struct = `sceMpeg` at +0, game fields after it): file extension picks the mode (0x2B2F50
  ".m2" -> mode 2 at +0x140). It reads the first 0x4000 bytes through its read callback (+0x60) and demuxes them
  (`sceMpegDemuxPss`, 0x2740D0 -> 0x273CF0); its video stream callback 0x18A788 copies the elementary stream into the
  ring at +0xE8 (+0xF0 write position). It then reads the picture size from the sequence header at the ring start
  (+0x58 width, +0x5C height), allocates the picture buffers (+0x94[], (w/16)*(h/16)*1024 bytes) and calls
  `sceMpegCreate`. Stream callbacks: type 0 (M2V) 0x18A788, type 2 (PCM, channel 0) 0x18A8F8.
- Per frame (0x18A170): `sceMpegIsEnd`, then `sceMpegGetPicture(mp, +0x94[i], mbcount)`; the picture is uploaded as
  40 strips of 16x480 to TBP 0x2800 (TBW 12) and drawn as a sprite into fbp 104 (TEST = 0, ZTE off).
- The movie's VBlank handler 0x26CF20 calls 0x26E798 (PMODE/SMODE2/DISPLAY1/2/DISPFB1/2 stores) and kicks the upload
  and draw packets.

## Open
- **Movie audio is silent** (audio check port/fmvc3audio: menu music, then silence for the whole movie, then the
  gameplay sound). The audio callback 0x18A8F8 is called (it skips a 4-byte substream header, keeps the first 0x28
  bytes as the stream header at +0x10C and copies the rest into the audio ring +0xFC, size +0x100, write +0x104/+0x108;
  ring full sets +0x134 = 1). What drains that ring to the IOP / sound engine is not identified yet (start command:
  SMPD 0x1C from 0x188E80). Note the stub's stream-type constants (PCM 1, ADPCM 2) differ from libmpeg's
  (M2V 0, IPU 1, PCM 2, ADPCM 3, DATA 4); the callback matches only because the stub delivers audio for both.
- The movie code still stores to IPU / DMA channel 3/4 registers (they reach no device; no IPU model exists).
- Other movies than the intro are not checked yet.
