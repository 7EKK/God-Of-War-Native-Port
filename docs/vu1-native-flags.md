# VU1 native blocks: flag latency across blocks (character shadows)

Character shadows are drawn by a shadow-volume pass: the game clears G to 127, inverts Z, draws the volume twice
(front faces G += 255 = -1, back faces G += 1, COLCLAMP=0 wraps), copies G's top bits to alpha (8 px CT16 strips)
and darkens where the count is not zero (DATE). The two halves differ only by the ADC bits the VU1 culling loop
writes: e.g. at 0x3C38 `FMEQ vi8` tests the sign of the `MSUBy.x` issued exactly 4 pairs earlier, across the loop
branch, and `ISW.w` stores the ADC word.

## Why there are no character shadows with the default native backend
The native blocks (tools/vu1/vu1n_gen.py, ps2_vu1_native.inl) commit every in-flight flag at native entry and a
block's newest flag writer at its exit. The loop above then reads the flags of its last pair: both halves of the
volume are culled alike (Desert: 332 / 332 triangles; PCSX2 311 / 381, PCSX2's interpreter in our build 282 / 370),
every pixel nets to zero. The exact engine (`GOW_VU1_SIN_COMPILAR=1 GOW_VU1_NATIVE_OFF=1`) draws the shadows.

## GOW_VU1N_FLAG_LATENCY=1 (opt-in)
- The generator emits every block twice: the former flag model (`<name>_f`, used by default) and the latency model.
- Latency model: flags older than a block come from per-flag rings of the last four writers (issue cycle, value;
  `VU1NativeAccess::macAt / clipAt / pushMac / pushClip`, one FMAC/CLIP per cycle so four cover the 4-cycle latency);
  CLIP chains from `m_workingClip`. The rings are exchanged with the exact engine's flag pipeline when control passes
  between the engines (`importFlags / exportFlags`).
- Blocks that read flags older than themselves (marked by the generator) get their exact issue cycles from the
  pipeline state at entry (`blockTiming`: in-flight lanes as a bitmask + ready cycles; idle table when nothing they
  read is in flight, else the exact stall model once per distinct entry state, cached). Every block hands its results
  that outlive it on (`finishBlockTiming`, tail lanes) and the exact engine's readiness arrays are synced when it
  continues (`syncInflight`).
- Result: shadows like the exact engine and PCSX2, no artefacts; but VU1 costs about 2x (Olympus, end of the zone
  walk: 5.2 ns per VU1 cycle vs 2.5 on main, ~39 fps vs ~56). Default stays the former model until that is solved.
- Open: make the latency model cheap enough for 60 fps (profile with a debug-info build; the remaining cost is in the
  generated latency-model code and the per-block bookkeeping).

## Verification
`GOW_VU1N_VERIFY` compares native with the exact engine per run; integer-like words (GIF data, the ADC bit) are now
compared exactly (the float tolerance hid flipped ADC bits) and bisection targets ADC mismatches.

Diagnostics: `GOW_GS_HW_VOLTRACE=<n>` (triangles per half, G after the volume), `GOW_VU1_KICKPRIM=<prim>`,
`GOW_VU1_FMANDPC=<pc>` (ours and PCSX2's interpreter), `GOW_VU1_STARTMAC=1`, `GOW_VU1N_VERIFY` (`GOW_VU1N_DUMP=1`
writes the run's micro memory), RT dumps also write alpha (`_a.pgm`).
