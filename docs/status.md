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
