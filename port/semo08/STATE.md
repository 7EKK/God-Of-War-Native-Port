# agent/semo08 (SEMO08 A/V drift) - 2026-10-16

Worktree ext/PS2Recomp-kl-semo, branch agent/semo08 from gow-usa-kl 0ddab33; runner = copy of ext/PS2Recomp-kl
runner (unchanged); build out/build-semo (same options as build-kl: AGRESSIVE_LOGS/DEBUG_UI/IOP_RPC_TRACE OFF;
the CMake defaults turn them on and make movies run at ~36 pictures/s).

## Done
- Cause: the stub's picture clock (MPEG.cpp presentationTickForFrame) moved later at every late GOP start
  (125 x 2-4 VSyncs = 4.7 s). Fix d6de6e6: late pictures keep the PTS schedule; only a 0.5 s stall re-bases.
- Diagnostics kept in d6de6e6: GOW_MPEG_PACE (per picture PTS step/tick/target), GOW_SND_STALE (stale IOP ring blocks).
- Before (base/, main exe): 29.35 pictures/s, drift 4.45 s, content 4.78 s behind at 206 s.
- After (final/): 30.00 pictures/s, drift 0.21 s (ok), content 0.77 s behind at 201 s.
- others/ (partial, run killed by low system memory): SEMO05 0.12 ok, SEMO09 0.11 ok, SEMO10 0.37 bad (0.22 before;
  machine loaded), A01MP05 ok, A01MP02 cut by the kill.

## Not done
- Gate (zones 0-4, savetest) not run: the session stopped long runs for low memory.
- SEMO10 recheck; full movies.py batch.
- Remaining 0.77 s: 16 KB movie reads complete 2 VSyncs after issue (docs/fmv.md Open), audio ring runs dry.

## Data
base/ (main exe), diag*/ pace*/ rd/ nd/ dm/ (diagnostic builds, not comparable for speed), fix1/, final/, others/;
*_raw.pcm are GOW_AUDIO_RAW recordings (not committed).
