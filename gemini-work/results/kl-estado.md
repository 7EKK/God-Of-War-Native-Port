### Current State (5 lines)
1. Reaches state 11 ("Docks of Athens") with visible Kratos, combat, HUD, rain, and 3D geometry using 77 patches and 501 compiled VU1 microprograms.
2. Baseline clean gameplay perf: **7.58 vid::Flip/s** (57.71 host Hz) with synchronous FINISH, OpenGL GPU backend, and RAM presentation.
3. Presentation comparison (commit `343404f`): **6.91 FPS** with shared presentation (`PS2X_GS_DIRECT_PRESENT=1`) vs **6.85 FPS** with RAM presentation.
4. HW fragment discard: **7.27 FPS** with `PS2X_GS_HW_DISCARD_UNCOVERED=1` vs **6.96 FPS** with discard disabled (`0`).
5. Candidate setting: **11.1–11.4 FPS** reported for `GOW_GS_FINISH_ASINCRONO=1` + `PS2X_GS_DIRECT_PRESENT=1` (marked **UNVERIFIED**; untested by them).

### Items Related to (1), (2), and (3)
- **(1) Kratos Upper Body / Compiled VU1**:
  - *Patches*: `ps2recomp-vu1-sticky-preserve.patch`, `ps2recomp-vu1-budget-diag.patch`, `ps2recomp-vu0-macro.patch`, `ps2recomp-mmi.patch`, `ps2recomp-spr-dma.patch`, `ps2recomp-vu1-efu-opcodes.patch`.
  - *Env vars*: `GOW_VU1_BUDGET_DIAG=1`, `GOW_CAMERA_DIAG=1`.
  - *Files / Commits*: PR #18 (`bfee1bb`), PR #23 (`5e09b12`, `754c970`), PR #30 (`ca69e9b`), commit `247a9b3`, `ps2_runtime_macros.h`, `mmi_translation_helpers.cpp`, `camera_probe_test.cpp`, `renView::SetupPipeline`.
- **(2) Game FPS ~20 (Target >= 25)**:
  - *Env vars*: `PS2X_GS_DIRECT_PRESENT=1`, `GOW_GS_FINISH_ASINCRONO=1`, `PS2X_GS_HW_DISCARD_UNCOVERED=0`/`1`, `PS2X_GS_GPU_PROF=1`, `GOW_PERF_DIAG=1`, `GOW_PERF_DIAG=frames`, `GOW_PERF_FRAME_PC=0x001837B8`.
  - *Patches*: `ps2recomp-iop-fast.patch`, `ps2recomp-gs-opengl.patch`, `ps2recomp-gs-presentation.patch`, `ps2recomp-gs-hardware-profile.patch`, `ps2recomp-gs-profile-async.patch`, `ps2recomp-gs-discard-uncovered.patch`, `ps2recomp-gs-region-pages.patch`.
  - *Files / Docs*: `RENDER_PRESENTACION_COMPARTIDA.md`, `RENDER_DESCARTE_FRAGMENTOS.md`, `RENDER_UPLOAD_RING.md`, `RENDIMIENTO_PERFIL_GPU.md`, `docs/RENDIMIENTO_PRESENTACION.md`, `scripts\probar_rendimiento.ps1`, `tools\rendimiento\resumir.py`.
- **(3) Framebuffer Haze / Channel Shuffle**:
  - *Env vars / CLI*: `PS2X_GS_FEEDBACK_SNAPSHOT=1`, `--snapshot-feedback`, `GOW_GS_REPLAY_TEXFLUSH=1`, `GOW_GS_REPLAY_STATE`, `GOW_GS_REPLAY_AFTER`, `GOW_GS_REPLAY_SECONDS`, `GOW_GS_REPLAY_TRACE`.
  - *Patches*: `ps2recomp-gs-feedback-snapshot.patch`, `ps2recomp-gs-target-alias.patch`, `ps2recomp-gs-coordinate-alias.patch`, `ps2recomp-gs-region-pages.patch`.
  - *Files / Docs*: `docs/FEEDBACK_BILINEAL_MINIMO.md`, `tools/render/comparar_feedback_gs.cpp`, `tools/render/generar_feedback_gs.cpp`, `scripts\compilar_replay_gs.cmd`.

### Known Bugs and Workarounds
- **VU1 Compiled Code**: Sticky flags lost in MADD subnormals (fixed: `ps2recomp-vu1-sticky-preserve.patch`); FTZ/DAZ lost underflow flags (`247a9b3`); inter-block jumps added in PR #30 (`ca69e9b`); 65536-cycle budget cutoffs logged by `GOW_VU1_BUDGET_DIAG=1`; VU0 macro wrote EE RAM address 0 instead of VU0 data mem (`ps2recomp-vu0-macro.patch`).
- **GS Worker**: FINISH async (`GOW_GS_FINISH_ASINCRONO=1`) is experimental (**UNVERIFIED** stability); capture replay dropped shared presentation; async shader compilation falls back to compute tiles until compiled (~2s); read-only Z batching caused corruption and was reverted; upload ring overwrite fixed via fences (`RENDER_UPLOAD_RING.md`).
- **Rendering Artifacts**: Framebuffer feedback loop reads un-flushed writes (PS2 8KB cache difference; workaround: `PS2X_GS_FEEDBACK_SNAPSHOT=1`); color/Z aliasing on same target (fixed: `ps2recomp-gs-target-alias.patch`); coordinate/wrap aliasing past FBW (fixed: `ps2recomp-gs-coordinate-alias.patch`); intro camera misframing torso/head remains an **OPEN BUG** despite finite matrices (`GOW_CAMERA_DIAG=1`).
