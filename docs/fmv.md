# FMV (movies)

Status: **movies play natively, at real speed, with their sound, and end; the game then continues.** Checked: the
intro (Semo01), Semo02 and Semo05 (through `GOW_MOVIE_OVERRIDE`), the new-game path (the intro, then the ship level
preloads a01mp05 and waits for its trigger while gameplay runs at 60 fps), and all 14 BONUS movies through the
Treasures-menu path (below). No hardware register is touched while a movie plays (`GOW_IO_HIST`). `play.cmd` plays the movies; zone and save
tests still set `GOW_SKIP_FMV=1` (it replaces the movie API: 0x188CA0 load, 0x188E80/ED8/EF0/F20, so the game
continues at once) to keep the runs short.

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
- **Real-time playback**: the movie thread never blocks; it polls its stream service (0x188B80, called only by the
  movie player 0x188848) as fast as it can. Guest time advances only by executed cycles, and the recompiled poll costs
  more host time per guest cycle than a real EE, so guest time, the IOP and with it the movie sound fell to 0.72x real
  time (pictures at ~22 fps instead of 30; measured with the sync line below). The override `gowMovieStreamService`
  calls `EeScheduler::catchUpToHost` before each poll: guest cycles of scheduled events whose host deadline has already
  passed are accounted (EE timers and IOP included), at most one field per call. Gameplay is not affected (there the
  guest is never behind the host clock).
- `GOW_MPEG_DIAG=1`: pictures handed to the game, `AddStrCallback` registrations, stream-callback call counts, and a
  `[MPEG:sync]` line every 30 pictures (PTS, wall time, audio device position played/produced, time spent in the
  decoder stubs) plus each stream's first audio packet PTS. `GOW_SND_DIAG=1` adds the device position to the LIBSD
  stream start (`BlockTrans ... mode=13`).
- `GOW_MOVIE_OVERRIDE=<name>` (debug): the first movie the game loads is replaced by that one (e.g. `Semo05`,
  `BONUS03`); the name is put on the guest stack and passed to the load function 0x188CA0 (which copies it into the
  movie struct), so any length up to 31 characters works.
- `GOW_BONUS_MOVIE=<NN>` (debug): plays bonus movie NN from the title menu exactly as the Treasures menu does, without
  unlocking it (`GOW_BONUS_MOVIE_DELAY`, default 10 s after the menu update first runs; give a pad script, e.g.
  `GOW_PAD_GUION=900:select`, so the default test presses do not start a new game). The menu's command handler
  0x1BA2C8 writes the two digits into the game's "BonusXX" buffer (0x29CC00) and sets the menu object's movie name
  (+0x1518) and +0x1520; the menu update 0x1BA4F8 then calls 0x1B2D20(name, 3, 1) (the scripted movie request,
  played by 0x1B2958 -> 0x188CA0) and returns to the menu when it ends. Logs `[gow-fmv] menu sees movie state a -> b`
  (0x29C838) for the test scripts.

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

## Movie audio (how it reaches the speakers)
- The PSS audio is private stream 1, sub-stream header `FF A0 00 00` (0xAn = PCM channel n), then the stream header
  `SShd` (type 1 = PCM16 little endian, 48000 Hz, 2 channels, interleave 0x200) + `SSbd`, then PCM in blocks of
  512 bytes left / 512 bytes right. Packets are 4096 bytes on disc; the PCM runs on across packets of odd lengths.
- The stub hands each audio packet (type PCM = 2, ADPCM = 3, libmpeg's constants) to the game's callback 0x18A8F8
  from its own copy of the demuxed bytes, through a guest buffer (the game reuses its 16 KB read buffer). The callback
  keeps the first 0x28 bytes (+0x10C) and appends the rest to the 100 KB audio ring (+0xFC base, +0x100 size,
  +0x104 read, +0x108 fill); when the ring is full it returns 0 and sets +0x134. libmpeg stops demuxing there; the
  stub keeps refused packets, in order, and hands them over first on the next demux call.
- The movie thread (0x189020) sets up a 989snd PCM stream (0x1882E8: SNDRPC 0x3B, IOP ring 0x4000 bytes at 0x77640;
  0x188408: 0x3E start) and on every pass drains the EE ring into the IOP ring (0x189A88 -> 0x188380 -> 0x188688:
  989snd command 0x40 = `sceSdBlockTransStatus`, minus the IOP ring address = where the sound unit reads; then SIF
  DMA in 1 KB units). 989snd plays the IOP ring with `sceSdBlockTrans(core 1, mode 0x13 loop)` = the core input
  stream of the native SoundEngine, mixed at BVOL 0x7FFE with MMIX 0x0FCF.
- Fixed for this (2026-10-12): `sceMpegCreate` resets the sceMpeg's callbacks (the game registers its stream callbacks
  before Create for the first demux and again after it: both stayed active and every audio packet went into the ring
  twice); `sceSdBlockTransStatus` returns the IOP address read (+ ring half in bit 24) like libsd, not an offset
  (the game's free-space count went negative and nothing was sent); BVOL is a plain 16-bit level (it was converted
  like a voice volume: 0x7FFE became -2, the stream was mixed at about -80 dB).
- Check: port/fmvc4v (`GOW_AUDIO_PCM`): the movie section (35-80 s) has sound with the smoothness of real audio
  (first-difference / RMS 0.06-0.28, the menu music measures 0.1-0.2; it was 1.3-1.4 = noise before).

## A/V sync (intro, port/c5/sync5)
- Picture PTS follows wall time (6.01 s of PTS per 6.00 s of wall; drift = the 29.97/30 rate), and the device plays
  exactly what the engine produces (no underruns). Semo02: 113.1 s PTS in 113.0 s wall, Semo05: 110.1 in 110.0.
- Audio and video start at the same PTS (8329). When a picture is handed to the game, the audio produced so far is
  ~90 ms behind its PTS (picture 30: 1.094 s vs 1.0225 s; picture 600: 20.113 s vs 20.0225 s), constant over the
  movie; the device buffer adds ~20 ms and the picture is shown 1-2 VBlanks after it is handed over, so what is heard
  trails what is seen by roughly 0.1 s.

## BONUS movies (Treasures menu, port/movies2)
The disc has 14: BONUS01, 03, 06, 10, 12, 14, 16, 17, 18, 19, 20 (the credits), 21, 24, 25 (01/19/20/21 on layer 1).
Each was played with `GOW_BONUS_MOVIE` from the title menu, with sound and a `GOW_AUDIO_PCM` dump
(`port/movies2/run_bonus_all.sh`, results in `bonus_all.txt`, contact sheet `bonus_sheet.png`):
- pictures at 30.00 per wall second in every movie (14 s for BONUS19 up to 812 s for BONUS03);
- sound for the whole movie: produced audio = picture time, RMS 3.6k-13k, smooth (first difference / RMS 0.01-0.83,
  noise measures 1.3-1.4), at most 5 silent one-second windows (fades);
- every movie ends (movie thread 11 -> 0) and the game is back at the title menu; no runtime errors.

## Hardware registers (none while a movie plays)
The player's own display/DMA layer (0x26CF20 VBlank handler -> 0x26D048, 0x26E798 display setup, 0x26ED68 / 0x26EDE0
normal / chain DMA sends, 0x26ED20 D_SQWC, 0x26CB90) and the open/close code (0x189BD0, 0x18A170: D_ENABLEW and the
IPU channels) used to go through the generic hardware path: per picture ~3 GS display stores (SMODE2, DISPLAY1/2,
PMODE, DISPFB1/2), a CSR read and 3 DMA sends (VIF1, GIF x2) with their D_STAT clears. Now (docs/runtime-changes.md
"Movie path without hardware registers"): display stores are `gsDisplayStore` calls on the native display service,
the CSR read is `gsStatusLoad`, the DMA stores go to the native DMA request service (VIF1/GIF packets sent at once;
D_ENABLEW dropped; fromIPU/toIPU starts have nothing to move). `GOW_IO_HIST` during BONUS17: 0 accesses (before, during
BONUS03: ~11k each of SMODE2/DISPLAY1/DISPLAY2 per 400 s, plus the DMA registers).

## Open
- In-level movies (a01mp05...) are checked only through `GOW_MOVIE_OVERRIDE` (same player); the level trigger was not
  reached by the scripted walk.
