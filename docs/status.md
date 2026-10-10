# Status (updated 2026-10-09, evening)

## How to resume (saved 2026-10-09 ~13:30)
Everything is committed **locally** (nothing pushed anywhere):
- Project folder `God Of War/` is a git repo: docs, tools, configs, overrides (commit 598a416+). Game data,
  ISOs, archives, ext/, ref/, generated code, logs and dumps are git-ignored on purpose.
- `ext/PS2Recomp` branch **gow-pal-port**, commit f18bbb3: all our runtime/IOP/recompiler work (PAL stack).
  The generated runner file register_functions.cpp is left uncommitted on purpose.
- `ext/PS2Recomp-kl` (worktree) branch **gow-usa-kl**, commit ce69fd7: c5a9d02 + KIexster's 68 patches.
Rebuild the USA build from scratch:
1. `ext/PS2Recomp-kl/out/build-kl/ps2xRecomp/Release/ps2_recomp.exe port/usa/recomp.toml` → port/output-usa
   (port/usa/ = their config/funcmap.csv + recomp.template.toml with @ELF@/@MAP@/@OUT@ filled; recreate from
   ref/other-recomp if missing).
2. Copy port/output-usa/* + ref/other-recomp/.../src/gow_overrides.cpp + src/*.h into
   ext/PS2Recomp-kl/ps2xRuntime/src/runner/ (delete old files there first).
3. `cmake --build out/build-kl --config Release --target ps2EntryRunner` in ext/PS2Recomp-kl (~30-40 min full).
4. Run (cwd = exe dir) as in the "First USA run" line below; captures gow_pad_test_N.ppm → tools/ppm2png.py.
Next steps, in order: (a) review/finish loading the user's save (port/usa3 run was interrupted);
(b) performance plan (section at the end); (c) port DiscSource (read extracted disc-usa/ instead of the ISO);
(d) audio (SPU2 output patches exist in their stack); (e) Switch build.

## Direction change (2026-10-09, afternoon)
The user supplied (1) KIexster's god-of-war-recomp project (ref/other-recomp): 68 PS2Recomp patches + config +
overrides that reach USA gameplay (Kratos, HUD, "Docks of Athens", memory card load/save, FMV, ~2 fps);
(2) all game editions (USA/Japan/Asia/Korea zips); (3) a PCSX2 memory card (ref/saves/Mcd002.ps2) holding a
USA save (BASCUS-97399GodOfWar).
**New primary target: USA (SCUS-97399) on their patch stack**, plus our own fixes where theirs lack them.
- ext/PS2Recomp-kl (built OK 2026-10-09 ~12:40, out/build-kl/ps2xRuntime/Release/ps2EntryRunner.exe): git worktree of PS2Recomp at their pinned commit c5a9d02 with all 68 patches applied in
  their order (tools/kl_patch_order.py prints it); build dir out/build-kl (VS generator, Release).
- disc-usa/: USA disc extracted (+ .disc_layout, layer1=2080544), IOP_MOD/ copies of the IRX, Mcd001.ps2 =
  copy of the user's card. port/usa/: their funcmap.csv + recomp.toml; generated code in port/output-usa.
- They independently found the same recompiler bugs (64-bit branches, SQRT.S ft, RSQRT.S, CVT.W, sin/fabs/floor
  stubs). Their checkpoint patch fixes the null virtual call that crashes our PAL build at 0x180E20.
- Still ours to port onto their stack: extracted-file disc reading (DiscSource; they read the ISO via GOW_ISO —
  used temporarily for the first test), real-SIO2 controller fixes (module start() as IOP thread,
  sceSifSetDmaIntr; they use a libpad2 HLE instead), PCSX2 VU1/EE reference tools.
- Other user-supplied files (2026-10-09 12:1x), checked: GoW II (USA, SCUS_974.81) and GoW II "Colossus Battle"
  demo (SCUS_974.82): ELFs have **no symbol tables** (only .text/.data/.rodata/.bss). Kept in ref/gow2,
  ref/gow2demo for a possible later port. ps2dis.zip (PS2 disassembler) and PS2ClientLoader (loads ELFs on real
  hardware) are not needed now (capstone tools + PCSX2 cover this). Japan/Asia/Korea zips not extracted.
- Memory card ref/saves/Mcd002.ps2: PCSX2 8 MB card, folder BASCUS-97399GodOfWar (data0-4.bin, icon.sys,
  static.ico) = USA save. The PAL ELF does not contain its folder name as a string (built at runtime).
- **First USA run on their stack (2026-10-09 ~12:45, verified by frame captures):** title screen with fire and a
  correctly rendered Kratos face, then (scripted Start/X via GOW_PAD_TEST=1) a new game starts and the opening
  shows Kratos on the cliff, fully 3D and textured (port/usa1/gow_pad_test_3.png, _11.png). Run: cwd = exe dir,
  `GOW_PAD_TEST=1 GOW_SKIP_FMV=1 GOW_ISO=<usa iso> GOW_MC0=disc-usa/Mcd001.ps2 ps2EntryRunner.exe disc-usa\SCUS_973.99`
  (captures gow_pad_test_N.ppm beside the exe; convert with tools/ppm2png.py). 200 s run, no crashes.
- PAL (our original stack in ext/PS2Recomp, docs below) is paused; it now boots to the legal screen with real
  soft-float math and stops at the null JALR (0x180E20) their checkpoint patch addresses.

## What works (PC build, verified by running)
- Recompiled game (6482 functions, 0 recompiler errors) builds with MSVC and runs.
- Boot runs entirely from the extracted files in `disc/` (no ISO argument).
- Full boot: CRT, static constructors, IOP reboot path, all 8 game IRX modules load and run on the IOP interpreter,
  anti-tamper check `sceChk01` passes, "SONY COMPUTER ENTERTAINMENT EUROPE PRESENTS" + legal screen.
- Data loading: `R_PermU` package streams from PART1.PAK through SMPD_IOP (byte-exact), sound banks (GENERAL, …)
  upload through 989snd/LIBSD, 218 sound entries register, loading completes.
- Title screen: red "GOD OF WAR" logo renders, then the message
  "PLEASE INSERT AN ANALOG CONTROLLER (DUALSHOCK 2) INTO CONTROLLER PORT 1 TO CONTINUE" (text partly garbled).

## What does not work yet
1. ~~Controller~~ — working (see below).
2. **Font/2D rendering**: some glyphs missing or garbled, some menu items drawn as grey blocks.
3. **Speed**: ~7 M EE cycles/s on the title screen (~2.4 % of a real PS2's 294.9 MHz). During loading ~60 M/s.
   GS and VU1 are CPU-emulated in the runtime. Must become playable speed, on Switch too.
4. **Audio output**: none (SPU2 DMA only signals completion; no mixing/output).
5. Memory cards: MC2_D gets no SIO2 device (reports "no device" once SIO2 lands); saving untested.
6. Switch: runtime libs build for aarch64; full game NRO never built or run on hardware.

## Controller: WORKING (2026-10-09)
Verified by running: the pad is detected and configured (DualShock 2, mode 0x79), polled with full 21-byte reads
(~3 polls/s at current speed), the data reaches the game's libpad2 socket buffer (EE 0x2FFB00, identical layout to
PCSX2: `04121601 00000001 … FFFF 8080 8080`), and pressing ✕ on the title moves to "CHOOSE DIFFICULTY".
Root causes fixed (details in iop-changes.md):
1. IRX module `start()` ran without an IOP thread → SIO2D/DS2U_D's WaitEventFlag/WaitSema during init fell through
   and left SIO2MAN's worker one transaction out of phase forever (replies read before they arrived, port-1 record
   got port-0 data, DS2U_D polled the empty port). Now `start()` runs as a transient thread.
2. sifman 32 `sceSifSetDmaIntr` was unimplemented → DBCMAN never DMA'd pad data to the EE.
3. dmacman ordinals (28/32), SIO2 RECV1 composition / port bit / DMA padding aligned with PCSX2.
4. Keyboard was ignored whenever any gamepad was attached (ps2_pad.cpp); now both are merged.
Test input without focus: `tools/key.ps1 ps2EntryRunner <delay> <holdMs> <vk> <scan> [repeat]`
(Enter = 13 28, X = 88 45).

## Active task: rendering — 2D correct, 3D geometry still wrong
Method (2026-10-09): `PS2X_GS_RECORD` writes our GIF stream as a PCSX2 .gs dump; PCSX2 replays it. PCSX2 drew the
same garbage → the GS rasterizer is not the main problem, the data fed to it is. Findings/fixes so far:
1. VU1 now runs **PCSX2's VU1 interpreter** (ps2xRuntime/src/lib/vu_pcsx2/, default; `PS2X_VU1_LEGACY=1` = old
   VU1Interpreter). Old VU1 aborted microprograms on GIF tags with FLG=3 (valid: = IMAGE) → title logo, text and
   fire background now correct.
2. GS frontend: FLG=3 treated as IMAGE; PRE/PRIM only in PACKED mode (both per PCSX2).
3. VIF1 UNPACK aligned with PCSX2 Vif_Unpack.cpp: V2 writes x,y,x,y; V3 takes w from the next element; V4-5
   expands RGBA5551 to 8-bit; MODE 3 stores data in ROW. No UNPACK is ever cut by a buffer end (logged if so).
4. Recompiler FPU: `rsqrt.s fd,fs,ft` was translated as 1/sqrt(fs) (ignored ft) → now fs/sqrt(|ft|); `div.s` by
   zero gave ±INF → now sign|FLT_MAX, overflow clamped (PCSX2 FPU.cpp). VU0 VDIV/VSQRT/VRSQRT likewise.
   (Rebuild after regenerating port/output — result not yet checked at the time of writing.)
5. GS verified with known-good input: a PCSX2 GS dump of the real title (Shift+F8 in PCSX2, `ref/pcsx2/snaps/`)
   replayed through OUR GS (`PS2X_GS_PLAY`) draws Kratos' face, text and layout correctly → our GS geometry is fine.
6. Bone/model matrices in EE RAM (e.g. 0x3F3F90, 0x450790; same addresses in PCSX2) differ from PCSX2's (scale ~2
   vs ~0.1, translation ~-1000 vs ~10). Writer found with `PS2X_EE_WATCH`: function 0x159108 (stores at
   0x159734..0x1597AC) normalises rows with `sqrt.s`. **Recompiler bug: R5900 SQRT.S takes its operand from ft,
   the translation used fs** (e.g. 0x46010044 = sqrt.s $f1,$f1 computed sqrt($f0)). Fixed in fpu_translator.cpp.
7. Matrix comparison caveat: PCSX2's snapshot was ~25 s into the title, ours at its start, so the bone matrices
   at 0x3F3F90 are not yet an apples-to-apples comparison (needs the same game frame).
8. ps2_runtime_macros.h: CVT.W.S truncation (was round-to-nearest) + FPU/VU0 clamping (full rebuild).
9. **Main 3D bug found (2026-10-09)**: gow.toml stubbed `sin`, `fabs`, `floor` with the runtime's native versions,
   which take a float in $f12 and return in $f0. This game's libm is **double-precision soft-float**: argument
   is a 64-bit double in $a0, result in $v0 (verified by disassembling 0x287FE8/0x289A00/0x289A38). Every sin()
   returned garbage → wrong rotation/bone/camera matrices. Removed the three stubs so the game's own code runs.
   Found with the EE differential tester (`PS2X_EE_DIFF=all`, see diagnostics.md): 1659 functions identical to
   PCSX2's interpreter, the differing ones traced to stubbed library functions.
10. With sin/fabs/floor un-stubbed the game crashed (null JALR at 0x180E20) because its soft-float double code
    hit a recompiler bug: 32-bit sign test in 64-bit branches (see elf-and-recompilation.md). Fixed.
Evidence for the remaining 3D bug (before fix 6): VU1's XGKICK packets contain vertices with Q < 0 and ±3.4e38 coordinates, i.e.
the EE-side matrices/vertices are wrong (VU1 itself is PCSX2's interpreter now).
Screenshots: window capture (`tools/shot.ps1`) returns white when the host display sleeps — use
`PS2X_FRAME_DUMP=<file.ppm>` + `tools/ppm2png.py` instead.

## New references (2026-10-09, in `ref/`, not used at runtime)
- `ref/proto/`: "God of War — The Hydra Battle" demo disc prototype (Dec 23 2004, SCUS-974.67). Checked: the ELF
  has **no** symbol table / debug sections (sections: .text .data .rodata .bss + DVP overlays); its IRX modules
  have no symbols either (SIO2MAN byte-identical to retail). Only strings like `989snd.c`.
- `ref/pcsx2/`: PCSX2 v2.8.2 (Windows Qt build). No BIOS included — PCSX2 needs a BIOS dumped from a PS2.
  Intended use (user, 2026-10-09): study the game with PCSX2 instead of reverse-engineering by building our own
  emulation.

## Next steps after the controller (in order)
1. Font/2D rendering garbage (GS CPU backend: texture/CLUT/alpha path for 2D text) — compare against expected.
2. Get in-game; fix crashes/hangs as they appear (keep using the trace tools).
3. Performance: profile; GS/VU1 to GPU/faster paths; Release (/GL) build for measurement.
4. Audio: SPU2 output.
5. Switch NRO build + on-device test (see switch.md).

## Uncommitted state
All PS2Recomp changes are uncommitted in `ext/PS2Recomp` (39 modified files + new files: disc_source, iop_format,
iop_sio2, ps2_switch_runtime.cpp, switch/). Nothing was committed or pushed.
- USA menu after Start: vertical list "NEW GAME" with ▼ (port/usa2/gow_pad_test_2.png); Down at 13 s after first pad read was too early (still New Game at 18 s) → new game started again (cliff intro).

## Performance work (started 2026-10-09 ~13:10, user request) — USA on the KIexster stack
Their measurements (ref/other-recomp/.../docs/RENDIMIENTO.md): VU1 interpreter dominated (>80 % of the game
thread after an IOP getenv fix); with OpenGL GS, compiled VU1 microcode (501 captured micro-memories), SSE FMAC,
IOP scheduler and VIF1 fast unpack they reach ~7-14 vid::Flip/s in loaded gameplay (profile: VU1 49 %, GS incl.
waits 31 %, EE 14 %, IOP 6 %).
Plan:
1. Runtime switches, no rebuild: `PS2X_GS_GPU=1` (OpenGL GS; `PS2X_GS_THREAD=1` = CPU GS on a thread),
   optional `GOW_GS_FINISH_ASINCRONO=1`. Measure with `GOW_PERF_DIAG`.
2. Capture VU1 micro-memories with `GOW_VU1_CAPTURA=<dir>` over title/intro/gameplay, then rebuild with
   `-DPS2X_VU1_MICROCODE_DIR=<dir> -DPS2X_VU1_GENERATOR_SOURCE=<their>/tools/vu1/generar_vu1.cpp`
   (`GOW_VU1_SIN_COMPILAR=1` disables it at run time).
3. Profile the rest (their tools/perfil/muestrear.cpp sampling profiler; needs /Zi build).
Measurements must run with no compiler or other game instance active.
- 2026-10-09 ~13:15: stopped by the user while the 2nd load-game test (port/usa3.log, GOW_PAD_GUION start@5,abajo@16,x@24...) was running; its captures were not reviewed. Performance step 1 (OpenGL + VU1 capture) not started yet.
- 2026-10-09 (resumed session): reviewed port/usa3 captures (converted to port/usa3/*.png). Menu → LOAD → X:
  **the user's memory card is read correctly**: "Select Load file from Memory Card Slot 1" lists 4 saves with
  thumbnails (first: OLYMPUS, Sep 27 2021 23:54:43, Hero (Normal) 10:11:50) — visible from ~90 s (state=5) to the
  end (240 s). The scripted X presses ended at ~90 s, so no save was selected yet. Next: rerun with X presses after
  ~100 s (port/usa4).
- 2026-10-09: their configureGowCdImage() (gow_overrides.cpp) uses GOW_ISO, else "<elf dir>/../God of War.iso",
  else the first .iso in the ELF folder, **else an IOP "virtual ISO" built from the extracted folder**. So step
  (c) (DiscSource port) may be unnecessary — to verify with a run without GOW_ISO (port/usa5).
- port/usa4 (running): GOW_PAD_GUION='5:start,16:abajo,24:x,105:x,125:x,145:x,175:x,205:x,250:x', 620 s.
- **2026-10-09: user's USA save loads and plays (port/usa4, verified by captures).** Script above: save list at
  ~90 s, "Load Complete." at 190 s (state=5→6), gameplay at 360 s (state=11, flashReady=1): Kratos in Olympus at
  the save altar, R2 prompt, scene fully textured/lit (port/usa4/gow_pad_test_14.png). Next step (a) done.
  User now asked (2026-10-09): **performance, target ≥ 60 fps**. Starting performance plan step 1–2.
- 2026-10-09 perf1 (port/perf1.log): OpenGL GS (PS2X_GS_GPU=1) + GOW_PERF_DIAG + GOW_VU1_CAPTURA=port/vu1_micro,
  same save-load script. Title ~5-5.8 game frames/s; loaded gameplay (state=11, ~375 s) **4.0 game frames/s**
  (host presents 60/s). Per 5 s window in gameplay: VU 3.07 s, EE 1.58 s, GS 0.20 s, IOP 0.15 s → VU1 is ~61 %.
  (Capture adds overhead.) 351 VU1 micro-memories captured (5.7 MB). Next: reconfigure out/build-kl with
  -DPS2X_VU1_MICROCODE_DIR=port/vu1_micro -DPS2X_VU1_GENERATOR_SOURCE=ref/other-recomp/.../tools/vu1/generar_vu1.cpp,
  rebuild, re-measure. 60 fps needs ~15x: requires a real VU1 recompiler (microVU-style) and EE speedups.
  Stopped here by the user.
- 2026-10-09 perf analysis of perf1 gameplay windows: the game thread is saturated (ee+vu+gs+iop ≈ 4.98 s per 5 s);
  per game frame ≈ 250 ms = VU1 155 + EE 77 + GS 10 + IOP 7. 60 fps = 16.7 ms total. host_wait (~4.95 s) is the
  presentation thread, not a bottleneck.
- Rebuilding out/build-kl with compiled VU1 (PS2X_VU1_MICROCODE_DIR=port/vu1_micro, 351 images) **and /Zi + /DEBUG**
  (still /O2) so their sampling profiler (tools/perfil/muestrear.cpp) can name hot functions. Log port/kl_build_vu1.log.
- agy note: first headless run produced no output — it tried a shell command (denied by settings). Prompt must say
  "Do NOT run shell commands; only read files and search the web".
- 2026-10-09 live play with the user's PS4 controller: it did not work — GLFW lists the user's sim-racing gear
  ("HBP Handbrake", HID 346E:001E/001F) as gamepads and ps2_pad.cpp read only slot 0. Fix (ps2_pad.cpp):
  the gamepad whose button was pressed last becomes active (press edges), else first available slot; logs
  "[pad] gamepad slot N: <name>". gow_overrides.cpp keyboard-stick fallback now only when no slot has a gamepad.
  Verified: "[pad] gamepad slot 0: Wireless Controller" (port/play3.log).
  Play config: PS2X_GS_GPU=1 PS2X_GS_WORKER=1 GOW_GS_FINISH_ASINCRONO=1 GOW_SKIP_FMV=1; title menu 30 fps.

## Bug hunt (2026-10-09 evening; user: pause optimization, fix purple fog, Kratos breaking when walking,
## music restarting; add 16:9)
- Save slots on the user's card: 1 OLYMPUS 10:11:50, **2 DESERT OF LOST SOULS 4:24:25 (purple fog visible)**,
  3 THE CLIFFS OF MADNESS 8:15:53, 4 FINAL BATTLE 10:02:09.
- New test tooling (runner gow_overrides.cpp, copy in port/overrides-usa/): GOW_PAD_GUION accepts stick moves
  (lup/ldown/lleft/lright, rup/...) with hold time "*<s>" (e.g. "170:lup*4"); GOW_PAD_CAPTURAS="t1,t2,..."
  sets capture times. IOP: GOW_SPU2_DIAG=1 (spu2.cpp) prints IRQ/IRQA, sound-RAM writes per 64 KB block and
  active voices every 2 s of SPU2 time.
- Reproduced (port/desert1, worker on): purple/blue-violet haze; Kratos' body mostly missing while walking
  (gow_pad_test_6/7), fine standing (_10).
- Music: gameplay audio repeats every ~6.08 s (port/audio1.pcm autocorrelation). SPU2 IRQ never enabled
  (ATTR bit6=0): the driver streams by polling; music voices v1.00/v1.01 (ssa 0x2820/0x3020) loop while sound
  RAM in that area is rewritten.
- 16:9: PCSX2 patches.zip has SCUS-97399_D6385328.pnach (enhanced widescreen code patch 0x169250-0x16944C +
  native widescreen flag 0x29CC90 and aspect floats).
- Parallel agents (user request) analysing music / fog / Kratos / widescreen; outputs in port/agents/<topic>/.
- 2026-10-09 ~19:55: agents put on standby by the user (token budget). Partial outputs, NOT reviewed yet:
  port/agents/fog/diag.diff, port/agents/widescreen/{runtime.diff,make_ws_variants.py}; music/kratos wrote no
  report. A/B run (gameplay-anchored script, worker on = port/abA, off = port/abB) was running at standby; compare
  Kratos frames there first when resuming (tools/sheet.py). Next: finish A/B, review agent diffs, then fix.
- 2026-10-09 A/B gameplay-anchored (port/abA worker on, port/abB off, *_sheet.png): Kratos intact in both
  (24 frames); breakage is intermittent — needs denser capture. Sand haze correct (sandy) in A's sandstorm area.
- **16:9 done (GOW_WIDESCREEN=1)**: gow_overrides.cpp re-applies every frame the data part of PCSX2's pnach
  (flag 0x29CC90=1, floats 0x2FD930/0x2FD968/0x32F1B0 = 16/9) = the game's native widescreen (vertical crop);
  window opens 1280x720 and gs_host_present.cpp fits the image to 16:9. Also without it the image is now shown at
  4:3 (was square pixels, "1:1" per user). Verified port/ws1. Enhanced (wider FOV) variant needs the pnach code
  rewrite 0x169250-0x16944C + recompile — not done. Agent's widescreen runtime.diff was whole-file → unused.
- **Music restart FIXED (2026-10-09)**: SPU2 NAX read one block ahead → 989snd (polling streamer) aborted the
  music stream (error 0x35) on a ring half's last block. NAX now reported like PCSX2 NextA. Details
  port/agents/music/REPORT.md; verified by port/music2.pcm (no ~6 s repetition). User to confirm by ear.
- Purple fog (2026-10-09): fog agent found GS fog code = PCSX2 (port/agents/fog: diag.diff applied uncommitted in
  ext/PS2Recomp-kl — env PS2X_GS_FOG_DIAG=<s>, PS2X_GS_FOG_FORCE=RRGGBB, PS2X_GS_FOG_OFF=1). Run port/fog1:
  **the game uses no GS fog at all (74808 draws, 0 with FGE)**. The haze comes from full-screen ctx1 sprite
  passes on 16-bit views (psm=0x2) of the 32-bit framebuffer with FBMSK masks 0x00003FFF / 0xFFFF00FF = channel
  shuffle / depth-fog effects (PCSX2 HW special-cases these). Prime suspect: our CT16 reinterpretation/masking
  of the 32-bit target (GPU shaders vs CPU backend). Next: reach the purple canyon (desert1 recipe, absolute
  script) and compare PS2X_GS_GPU=0 (CPU) vs GPU; fogdiag lines identify the shuffle draws.

## How to resume (saved 2026-10-09 ~21:00)
- Build: ext/PS2Recomp-kl branch gow-usa-kl (all committed, author Waffle565; register_functions.cpp generated,
  never committed). Runner's gow_overrides.cpp is git-ignored: tracked copy port/overrides-usa/gow_overrides.cpp
  (copy it back before building if the runner file was replaced). out/build-kl: Release, /Zi, traces OFF,
  compiled VU1 from port/vu1_micro (841 images).
- Play: PS2X_GS_GPU=1 PS2X_GS_WORKER=1 GOW_GS_FINISH_ASINCRONO=1 GOW_SKIP_FMV=1 [GOW_WIDESCREEN=1]
  GOW_ISO=<usa iso> GOW_MC0=<copy of ref/saves/Mcd002.ps2>, cwd = exe dir. Test runs: tools/perf_run.sh
  (GOW_PAD_GUION with g-relative times and sticks, GOW_PAD_CAPTURAS), tools/sheet.py, tools/gsw_trace.py.
- Done this session: save loads; 4 → ~20-25 fps (traces off, compiled VU1, GS worker); PS4 pad selection;
  music restart fixed; 16:9 + 4:3 presentation.
- Open, in order: (1) purple fog = channel-shuffle passes (see above): reach the canyon with the desert1 absolute
  script, compare PS2X_GS_GPU=0 vs 1; (2) Kratos intermittent breakage while walking: dense captures; (3) enhanced
  widescreen (pnach code rewrite + recompile); (4) performance (VU1 block compiler, split GS frontend thread).
- Agents: max 1 in parallel, always with Gemini support (user, 2026-10-09).

## Session 2026-10-09 (late) — purple fog, continued
- The "desert1" script (`60:derecha`) actually loads save slot 3 **THE CLIFFS OF MADNESS**; its lavender/purple
  haze is in the canyon background there (port/desert1 captures). fog1/abA reached the desert (sandy haze, OK).
- Bug found (PCSX2 GSRendererSW.cpp:1476-1480): for 16-bit frame PSMs the 32-bit FBMSK must be compressed to 5551
  (0x00003FFF → 0x00FF, 0xFFFF00FF → 0xFC1F). Our general draw path is equivalent (masks in 8888 before packing),
  but both fast-clear paths (gs_cpu_backend.cpp clear sprite, gs_gpu_backend.cpp clear dispatch) used the raw low
  16 bits. Fixed with `GSInternal::frameMask16` (ps2_gs_common.h); not yet rebuilt/verified.
- Save list: cursor starts on slot 1 (Olympus); one `derecha` = slot 2 Desert, two = slot 3 Cliffs (port/cpu2).
  port/cpu1, cpu2 (CPU GS) landed in the desert interior (no purple there). Running port/cpu3: Cliffs with CPU GS.
- **Kratos breakage (user screenshots 2026-10-09 21:32, desert interior, slot 2)**: upper back/head missing while
  walking. Repro: save slot 2 (`85:derecha*0.3`), walk `g8:lup*4`, capture g9 (port/kr*; compare crops with
  tools/crop.py). Results: compiled VU1 broken (kr1 20.7 fps, kr4); VU1 interpreter OK (kr2, 8.6 fps);
  compiled pairs without blocks `GOW_VU1C_SIN_BLOQUES=1` OK (kr3, 14 fps); blocks with dead-flag elimination
  disabled (new diag `GOW_VU1C_FLAGS_VIVOS=1`) still broken (kr4); blocks + GS worker off still broken in some
  frames (kr5, 14 fps) → not a GS-worker race, the bug is in compiled VU1 **blocks**. RANGO bisect visual
  results were ambiguous. New diag `GOW_VU1C_VERIFICAR=1`: each compiled block (without XGKICK) is also run on
  the interpreter from the same state; mismatches print `[vu1c-verify] block <pc> ... <first difference>`.
- KIexster update (19acde6, 2026-10-09 evening): applied patches/ps2recomp-gs-discard-uncovered.patch (GPU HW
  raster discards fragments outside the triangle; PS2X_GS_HW_DISCARD_UNCOVERED=0 = old). Rest of the update is
  his tests/tools/docs (camera probe, GS replay/feedback tooling) — not taken.
- FPS: kr1 (play config + 1 capture/s) 20.7 game fps in the desert; kr4 37 fps at the run's end. User order:
  ≥ 25 fps after every fix.
- Verifier notes: copying the whole VU1Interpreter per block made the game crawl (0.4 fps, never left the title:
  port/ver1-3); now `GOW_VU1C_VERIFICAR=<N>` checks one block in N. MAC/status are not compared (lazy flag
  commit makes m_state.mac differ at block end without any reader — title blocks 0x2840/0x2898/0x2B90 had only
  MAC "differences" and contain no FMAND/FMEQ/FMOR). Blocks containing XGKICK are not verified.
- KIexster ESTADO.md (via Gemini, checked): his VU1 fixes sticky-preserve and FTZ underflow (247a9b3) are already
  in our tree; he reports 7-11 fps, so nothing to take for performance; intro camera torso/head misframing is
  an open bug on his side too (GOW_CAMERA_DIAG).
- **Kratos: narrowed to compiled VU1 blocks that contain XGKICK.** Sampled verifier (`GOW_VU1C_VERIFICAR=500`,
  port/ver4, ~230 s of gameplay, 28 fps): zero register/VU-memory/cycle differences in blocks without XGKICK.
  New diag `GOW_VU1C_EXCLUIR=xgkick` (blocks containing XGKICK run as single compiled pairs; `=flags` does the
  same for MAC/status readers): port/ex1 Kratos intact at g9-g11 (every other compiled run broke there) and
  **29.5 fps**. Next: verifier extended to XGKICK blocks (GIF submit silenced in the shadow run, in-flight XGKICK
  packet bytes compared) to find the exact difference.
- **Correction (2026-10-10)**: verifier with counters (port/ver6): >125 000 gameplay blocks checked (~75 000 with an
  XGKICK packet in flight), **0 differences** in registers, VU memory, cycles and XGKICK packet bytes → compiled
  VU1 blocks behave like the interpreter. Repeat of the exclusion run (port/ex2, 25-31 fps) still had one broken
  frame (g10): `GOW_VU1C_EXCLUIR=xgkick` only made it rarer; earlier "fixed" runs (kr2, kr3, ex1) were likely luck
  of an intermittent bug. In the broken frame a whole torso batch is missing (we see the inside of the model).
  Next: GS capture of a broken frame (PS2X_GS_RECORD) replayed in PCSX2 — tells whether the GIF stream lacks the
  torso (EE/VIF/VU side) or our GS drops it.
- FPS with the current build (play config, desert room, captures on): 28-31 game fps (port/ver4, ex1, ex2) ≥ 25.
- tools/crop.py: side-by-side 2x crops of the same region across captures.

## Priority (user, 2026-10-10)
Kratos bug, user's description: while walking his torso twists to the right progressively until it loops all
the way around and clips into the legs; arms end behind the back; idle sway, attacks, other animations are fine.
→ an EE-side skeletal/animation math bug (bone rotation accumulates/wraps), not missing geometry (consistent with
the VU1 verifier finding no difference). Only after this is fixed and no other apparent bugs remain: aim for
16:9 at 60 fps.

## Session 2026-10-10 — Kratos walk twist: EE differential testing
- **EE differential tester ported to the USA build** (ext/PS2Recomp-kl: src/lib/ee_ref/ee_diff.cpp + ee_ref/ +
  vu_pcsx2/ from the PAL stack, CMakeLists, main.cpp calls ps2InstallEeDiff after loadELF). Env:
  `PS2X_EE_DIFF=all|<hex,...>`, `PS2X_EE_DIFF_MAX=<n>` (calls per function), `PS2X_EE_DIFF_START=<s>` (only calls
  after s seconds, e.g. during a scripted walk), `PS2X_EE_DIFF_STEPS`.
- tools/eediff_filter.py (drops upper-64-bit-only GPR diffs, sorts by steps), tools/callgraph.py (jal call graph
  from the runner sources: does function X reach Y).
- **rand fix (gowRand in gow_overrides.cpp)**: rand@0x28C0C8 is stubbed in recomp.toml and the generic stub returned
  the host's 15-bit rand(). Game's newlib rand (ELF 0x28C0C8 + PCSX2 reference): seed at *(0x2A4294)+0x58,
  seed = seed*0x41C64E6D+0x3039, returns seed & 0x7FFFFFFF. Callers scaling by 2^31 got ~0 (0x23F350: 2.245 vs
  4.5e-05). Now exact.
- ed1 (all functions, first call after 118 s, before the fix): 279/2400 differing; real ones = rand callers
  (0x21B330 reaches rand via 0x184398→0x184348) + 1-3 ULP float noise from div.s/sqrt.s (PCSX2's interpreter
  computes via double; PCSX2 itself is not bit-exact to the PS2 and runs GoW fine → not the cause).
- ed2 (all, 3 calls each, after the rand fix): 5743 identical, 673 differing, none with a significant (>0.1 %)
  non-stack memory difference.
- ed3 (skeleton: CalcWorldMatrix 0x1303D0 + callees 136870,136990,138108,13DC78,13E090,13E158,13E180,185270 and
  CalcSkinHierarchy 0x137558, 300 calls each during the walk): 800 identical, 0 differing (0x137558 not called).
  Names/offsets from KIexster's gow_model_probe.h (skeleton +0x8C = 64-byte joint matrices, +0x86 root id).
- KIexster update c5c4ad9: perf patches dma-iop-timers-cache / vif1-ee-timers-fast do not apply on our tree
  (ps2_memory.cpp, ps2_vif1_interpreter.cpp context) — deferred.
- **Next step**: the twist is not in CalcWorldMatrix/its callees nor in first-call EE code. (1) Verify visually
  whether the rand fix changed the walk (play: walk ~30 s in open space, front camera). (2) Find the animation
  sampler: run `PS2X_EE_DIFF=all PS2X_EE_DIFF_MAX=50 PS2X_EE_DIFF_START=<walk start>` on a short run and look for
  non-ULP differences; also log Kratos' joint matrices (skeleton+0x8C) per frame while walking to see which joint
  rotates progressively, then find its writer with PS2X_EE_WATCH (not yet ported to the USA build).
- ed4 (all functions, 50 calls each during the walk): 7153 identical, 773 differing, 74 not comparable; no
  significant non-stack memory difference (large gaps only in dead scratch FPRs, e.g. 0x10FC88 chain f5..f10;
  0x20D420 = ULP). Open: which calls were "not comparable" (quiet mode does not print the stop reason).
- **Window 1920x1080 (2026-10-10)**: `GOW_WINDOW=<w>x<h>`; default 1920x1080 with GOW_WIDESCREEN=1 (was 1280x720),
  shrunk to the monitor if needed; image letterboxed at 4:3/16:9 (gs_host_present.cpp). port/hd1: 26-30 fps.
  This is only the window: the GS still renders at native PS2 resolution (~512x448). **Full-HD internal
  rendering** (user, next after the Kratos fix) needs an upscaled render-target path in the GPU backend
  (PCSX2 HW renderer / texture cache style) — a large feature, planned, not started.
- **Twist repro (port/tw1)**: save slot 2, `g8:ldown*16` (walk toward the camera): legs run forward, torso turned
  ~180° with blades raised behind — clearly visible in gow_pad_test_1.
- Eliminated (2026-10-10, all match PCSX2 / sane): EE code during the walk (ed6: ~16 000 calls after gameplay start,
  no significant difference; ed4/ed5 never reached gameplay — all-function hooks slow loading, start the diff after
  gameplay), libc stubs (__divdi3/memcmp/rand differ only in scratch regs), SPR DMA (no interleave mode used, no
  aborted chains; `[dma] SPR ... not handled` diag added), animation dt (0.033 s, GOW_FAST_BOOT diag now also in
  state 11), EE timers (T0 mode 0x082 = BUSCLK/256, read ~660/s; `PS2X_TIMER_DIAG=1`), no VU0 micro-programs
  (no VCALLMS in the ELF). Not comparable = kernel/syscalls/COP0 + DMA register code (ed7, now printed).
- Remaining suspects: data coming from the IOP side (disc-loaded animation data, pad bytes).
- **Next step**: ground truth from PCSX2. Our dump: port/ram_desert_g5.bin (`GOW_RAM_DUMP=g5:<file>`, standing in
  the desert room 5 s after load). Needed: PCSX2 save state (ref/pcsx2, USA ISO, card ref/saves/Mcd002.ps2, slot 2)
  standing in the same room → extract eeMemory.bin (7-Zip) → compare level/animation data regions (static data
  loaded from disc must be identical; differences = IOP/disc path bug).
- **PCSX2 ground truth obtained (automated, see diagnostics.md)**: port/pcsx2state_desert/eeMemory.bin (desert room,
  ~5 s after load) vs ours port/ram_desert_g5.bin. Code/static data identical; level heap identical except an
  allocation-order drift: ours has two extra 16-byte heap entries at 0x19A78C4 (hashes A757E542→01EEFF00,
  A183E323→019A3090), after which data is shifted by +0x1480 (up to ~0x1EF0000); 0x1200000-0x13A0000 is per-frame
  data. Next: check whether the shifted/extra allocations are timing-only (dump both at the same frame) or a real
  difference; then compare Kratos' skeleton/animation data (object+0x104 → skeleton, +0x8C joint matrices).
- PCSX2 visual ground truth (port/pcsx2_walk1.png): running toward the camera Kratos faces it, arms swing, blades on
  his back. Ours (port/tw1/gow_pad_test_1.png): torso turned 180°, blades raised behind → confirmed port bug.
- div.s/sqrt.s ULP gaps explained: our runtime divides/sqrts with round-to-nearest (ps2FpuDivideNearest) while other
  FPU ops run with the EE's chop mode = PCSX2 2.x defaults (separate division rounding). eeref lacks that split →
  its 1-ULP "differences" are reference artifacts, not port bugs.
- SPR normal-mode DMA advances MADR/SADR and zeroes QWC (checked), pad bytes centered (128) — fine.
- Hypothesis for next session: 180° twist that "loops around" = yaw interpolation taking the long way (angle at the
  ±π wrap). Next: locate Kratos' object/skeleton (KIexster probe: hook 0x159878, object+0x104 skeleton) and log the
  spine joint's local yaw per frame in ours vs PCSX2 (PINE reads while walking), then find the variable that feeds
  it and compare that variable between both.
- GOW_SKEL_DIAG=1 / GOW_SKEL_OBJECT=<hex> (gow_overrides, wraps CalcWorldMatrix 0x1303D0): port/sk1 lists only 6
  objects and KIexster's offsets (object+0x104 skeleton, +0x60 definition count) give garbage counts for this a0 →
  the CalcWorldMatrix argument is not his "object" struct. Candidate near the player: 0x739FC0 (skeleton 0x73BB50,
  joints 0x7910F0, pos 2414.7,514,6744.9). Next: confirm the struct (read his gow_model_probe callers / hook
  0x159878 model context) before logging per-joint yaw.
- Skeleton diag now hooks ProcessModel 0x157A60 (object = *(a1+0x18), as in KIexster's probe). port/sk2: object
  0x1112470 has 125 joints (skeleton 0x11143B0, joints 0x11297E0) — likely Kratos. port/sk3 (GOW_SKEL_OBJECT=
  1112470, walking): the matrices at skeleton+0x8C never change → bind pose, not the animated pose. Next: find the
  per-frame animated joint matrices (output of CalcWorldMatrix 0x1303D0 / CalcSkinHierarchy 0x137558, or the
  matrices DMA'd to VU1 for skinning) and log the spine yaw there; then read the same in PCSX2 via PINE.
- **Kratos twist — strongest lead so far (2026-10-10)**:
  - Write watchpoint ported: `PS2X_EE_WATCH=<hex>[:len]` (page guard, logs guest pc/ra; src/lib/ee_watch.cpp) and
    `PS2X_EE_WATCH_WORD=<hex>` (function-level). `GOW_RAM_DUMP=<t>;<t>...:<file>` (multiple dumps, folder must
    exist), `GOW_PAD_LOG=1` (pad packet: clean, no buttons while walking).
  - RAM dumps while walking (port/rdump/ram.0-6): float at 0x11BF5F4 runs 0→4.3 and loops = an animation
    PLAYBACK TIME (writer 0x116EF0: time = min(time + dt*rate, length)), in an anim controller (type ptr
    0x2C2D20, struct base 0x11BF5A4) created by 0x2520C8 with parameters copied from its parent (type 0x2BF1A0,
    built by 0x106328 from 0x100EA8: +0x1C = layer index s5, +0x24 = data byte (lookup 0x239228)+2).
  - PCSX2 ground truth (walking save states port/pcsx2walk2/3): objects of type 0x2BF1A0: PCSX2 43, ours 45;
    (index 1, flag 0x81): PCSX2 1, ours 3 → **our game creates two extra animation layers** (likely the
    raised-blades upper-body overlay). Heap layouts differ per session, match objects by type pointer, not address.
  - Next: find what makes 0x100EA8 create the extra layers (its loop/table input; watch the creation path and
    compare the table entries / owner objects with PCSX2).
  - Refinement: the extra layer owners in ours (0x11BE09C, 0x11BE168, header word 0x00020024 = small 2-joint
    animators, probably props) are not Kratos. The winding time at 0x11BF5F4 belongs to 0x11BE168 → not the
    twist. Kratos' animator = header 0x001E0024 (ours 0x7AE808, PCSX2 0x7A69A0): same layers (0, 1*) in both.
    Next: compare Kratos' animator layers' state (time/rate/weight fields of its two controllers, type 0x2C2D20,
    reached via owner → layer parent (type 0x2BF1A0, +0x10 = owner) → controller (+0x18/+0x1C pointers)) between
    PCSX2 walking states and ours.
- Kratos' own animator layers/controllers are identical to PCSX2 while walking (port/pcsx2walk3 vs rdump/ram.3).
- `GOW_VU1_PCSX2=1` (ps2_runtime.cpp: MSCAL/MSCNT on PCSX2's VU1 interpreter via vu_pcsx2/pcsx2_vu1_bridge, as in
  the PAL stack): port/pv1 still shows the twisted pose → **VU1 semantics ruled out** (both ours and PCSX2's VU1 give
  the same broken pose). Still open: where the bone matrices that reach VU1 get the wrong rotation. Next: dump the
  skinning matrix palette Kratos sends to VU1 (VIF1 UNPACK source in EE RAM) in ours vs PCSX2 while walking.
- Kratos animator object (ours 0x7AE808 / PCSX2 0x7A69A0, ±0x400 bytes): no differing float fields, only pointers.
  No math functions among the recompiler stubs (file/SIF/kernel only). So far everything upstream of the pose
  matches PCSX2; remaining unexplored: the per-joint pose evaluation output (joint matrix palette per frame) and the
  palette's transfer to VU1 (VIF1 UNPACK). Plan: find the palette address via VIF1 UNPACK sources while walking
  (PS2X VIF trace) and compare those matrices with PCSX2's at the same animation phase.

## Session end 2026-10-10 (morning) — state of the Kratos animation bug
- User screenshots 065930/065937: the same raised-arm/blade pose also appears for ONE frame in idle and during
  the Q/E (L1/R1) animation → likely one bug: a wrong pose sampled/blended occasionally (continuously while walking).
- Tried, no visual change (kept, they match PCSX2): VIF1 UNPACK STMOD 3 now stores data in ROW (PCSX2
  Vif_Unpack.cpp setVifRow); EE FPU mode on the GameThread = PCSX2 default (round toward zero + FTZ + DAZ;
  GOW_EE_FPU_NEAREST=1 = old). port/m3, port/chop1 look identical to before (pose deterministic).
- Checked OK: no VIF1 UNPACK cut by buffer ends (`[vif1] UNPACK cut` diag, port/vc1), only 2 EE threads (main +
  waiting sound/stream; `GOW_THREAD_DUMP=<t>`), caller-frame diffs are ULP only (`[eediff-callerframe]`), skipped
  "did not return normally" calls are wrapper artifacts (0x116EF0/0x117A50/0x1064F0 compared OK individually).
- KIexster cbb58c3 (pulled into ref/other-recomp-git): he works on the same thing — full model palette and
  skinning-buffer diagnostics, native joint/camera comparison against a paused PCSX2 EE reference
  (tools/render/pcsx2_ee_reference.py), optional VIF1/VU1 thread patch (PS2X_VU1_HILO=1). His ESTADO.md summary
  was not obtained (Gemini run stopped). **Next: read his palette/joint findings first**, then compare Kratos'
  joint palette (skinning buffer) with PCSX2 at the same animation frame.
- Remaining stage not yet verified against PCSX2: the per-frame joint palette itself (EE output just before VIF1).

## Session 2026-10-10 (midday) — invasive tests of the Kratos twist
- KIexster's ESTADO.md (cbb58c3) read: he has NOT found the cause either (only input palettes at ProcessModel, matches
  PCSX2 within ULP-level noise; skinning output/VIF still unverified). Nothing to borrow for the twist.
- Animation-state object (ours 0x8629C0 / PCSX2 0x862900, referenced from Kratos' animator) carries stale heap garbage
  in ours ('Mark' string at +8, a float rotation at +0xA4) where PCSX2 has zeros → hypothesis "uninitialised heap data".
- **Test: `GOW_HEAP_ZERO=1`** (gow_overrides.cpp, wraps USA heap alloc 0x13DC78, zero-fills every returned block;
  note tools/dis.py reads the PAL port/output, use port/output-usa for USA addresses). port/hz1: pose IDENTICAL to
  port/tw1 (torso twisted, blades raised) → **stale/uninitialised heap data ruled out**.
- Repro command used: `tools/perf_run.sh hz1 230 GOW_HEAP_ZERO=1 GOW_PAD_GUION='5:start,16:abajo,24:x,85:derecha*0.3,92:x,g8:ldown*16' GOW_PAD_CAPTURAS='g12,g16,g20'`
  (gameplay starts ~110 s; captures hz1/gow_pad_test_1.png).
- Next invasive step: dump the joint palette Kratos sends to VU1 (VIF1 UNPACK V4-32 blocks) and compare with the
  bind-pose/hierarchy; or bisect by forcing individual animation layers/controllers off to see which one carries the
  twisted torso.
- New diagnostics (all env-gated, off by default): `GOW_VIF_MATDUMP=<file>:<start>:<len>` (+[vifhist] histogram of UNPACK
  shapes), `GOW_SKEL_ANIM=1` (per model-palette object: joints moved/nonfinite every 90 calls), `GOW_SKEL_DUMP=<file>`
  with `GOW_SKEL_OBJECT=<hex>` (raw 125x64-byte palette per call). Heap addresses are stable between identical scripted
  runs. In the desert Kratos candidate = object 0x1AC38B0 (125 joints, pos 2433,512,6738) but skeleton+0x8C is not a flat
  matrix array for it (only joints 0-5 look like matrices) → the pointer layout differs per object; palette dump unfinished.
- VIF1 UNPACK reviewed against PCSX2 Vif_Unpack.cpp (mask/row/col, STMOD 1/2/3, V2/V4-5, cycle/fill): equivalent except
  V3 W lane (PCSX2 = next source word; ours = stale VU memory, harmless) and CL/WL==0 remap. Histogram shows the walk's
  UNPACKs are V4-16/V4-8 (vertex data) and V4-32 num=1 (headers); no big V4-32 matrix blocks.
- DMA chain walker reviewed: tag ids 0-7 match hardware. No tag with the SPR flag (bit 63) occurs on any channel
  (`[dma] chain tag with SPR flag` diag, port/spr1) → ruled out.
- Dense capture (port/sq1_sheet.png, 16 frames over 1.5 s of the walk toward the camera): the raised-arm/blade pose is
  CONSTANT while the legs animate → the upper body is frozen in one pose (not a periodic glitch): upper-body layer
  not advancing, or blended at the wrong weight with a static "ready" pose.
- Checked, no difference: pad packet (pressures, order), DMA tag semantics, VIF OFFSET/BASE/ITOP/MSCAL double buffer logic,
  UNPACK mask/mode. Animator node lists differ in ours vs PCSX2 (3 contiguous 0x1E0024 nodes vs 2 nodes 0xB0024 in
  PCSX2) — header meaning unknown; may just be heap layout.
- **Next (decisive, not yet done)**: log Kratos' layer controllers' time/weight EVERY frame in ours during the walk
  (controller type 0x2C2D20 via layer parent 0x2BF1A0 +0x18/+0x1C; parent for owner 0x8629C0 at 0x6FC4F0 in ours rdump)
  to see whether layer 1 (flag 0x81, upper body) advances and what weight the blend uses; compare with PCSX2 states over a
  few frames (PINE reads). If time advances and weight is ok, the sampled clip data (disc-loaded) is the suspect.
- **GPU translation cleared (2026-10-10)**: PS2X_GS_GPU=0 (CPU software GS, port/cg2, gameplay at 207 s) shows the SAME
  frozen raised-blade pose as the OpenGL backend → the bug is upstream of the GS (GIF data), not in the GPU translation.
- `GOW_CTRL_DIAG=1` (gow_overrides.cpp, wraps controller update 0x116EF0): every ~4000 calls prints per controller
  (heap 0x600000-0x900000) calls/time/rate. port/ct1: Kratos-area controllers in the static pool (0x6FAF64, 0x6FAFD4,
  0x6FB834/8A4/914/984, 0x7481E8) advance normally (rate 1.0); the same pool addresses exist in PCSX2 with identical layout,
  clip pointers (0x600680, 0x607E00, 0x608200 … static region, equal in both) and advancing times → controllers/clips
  match PCSX2. No "No exact recompiled function" errors in the run. So: inputs (controllers, clips, EE code, VU1, VIF,
  DMA, both GS backends) all match; the frozen pose must come from something not yet compared — candidates: VU0
  state across thread switches/interrupts (vf regs, Q/flags are not saved by our thread switch?), or blend weights
  computed from a game-state variable (speed/stick) that differs between runs.

## Session 2026-10-10 (afternoon) — long invasive hunt, state at end
Ruled out (all verified, see earlier bullets for method): GPU backend (CPU GS shows the same pose), stale heap, pad, DMA tag
semantics/SPR flag, VIF1 UNPACK/MPG/STROW (no cut data, no unhandled VIFcodes, CL=4 WL=1 stride writes OK), VU0 flag registers
(game never uses CFC2/CTC2/VCLIP), sine table (matches PCSX2 to 1 ULP), animation clips (byte-identical to PCSX2), controller
set/weights (same 43 controllers of the player animator 0x74ED74 at identical addresses, weights equal), sampler 0x10FC88
(803 calls diffed against PCSX2's interpreter: only stack garbage/ULP), recompiled-function coverage (no missing functions).
- **Player model = object 0x753E20 (103 joints)**, skeleton 0x755EF0 (+0x88 animator 0x74ED74, +0x8C world palette 0xD53910,
  def +0x60 = 0xD4A960, local matrices array 0xD4FE40). Same addresses in PCSX2 → direct comparison possible.
- **Palette finding**: walking in ours the arm/blade-chain joints (12-30 and 31-51) are 3-18 units from spine joint 3 (mean ~10),
  PCSX2 coherent snapshots (6 walking frames, port/pcx_S.npy) stay 7-11. Local joint translations are constant in both
  (rigid), so the extra excursion comes from the chain joints' ROTATIONS: in ours snapshots 1-2 the 19 chain joints each rotate
  ~100-150 deg (curled) vs <90 in PCSX2. The pose is wrong in the WORLD PALETTE (upstream of VU1).
- Tools: `GOW_SKEL_OBJECT=753e20 GOW_SKEL_DUMP=<file>` (per-call palette), `GOW_DMA_BLOCKDUMP`, `GOW_CTRL_DIAG`,
  `PS2X_EE_WATCH_FROM=<s>` (write watch after s s), `PS2X_EE_DIFF_EVERY=<n>`, VU0 ctl state compared in eediff.
  PCSX2 coherent snapshots: PINE save slot (`python snap.py <slot> <dir>` pattern in this session) + 7z extract; live PINE
  reads are NOT coherent (mixed frames). PCSX2 slot-1 save = Olympus (walk with stick keys W/A/S/D; ~3 s holds).
- Next: sample the chain clip (type 0x3221 clips edd800..ee3d40 at 0xED....) offline with the game's own decoder and compare
  with the palette in both; or diff the blend (0x10A530/0x10B0C8 callers of the sampler) with PS2X_EE_DIFF_EVERY over a long walk.
