### 1. Summary of the 8 New Patches

#### 1. `gs-upload-ring` (`ps2recomp-gs-upload-ring.patch`, Patch #69)
* **What it does:** Hardens the OpenGL 64 MiB persistent upload ring buffer (divided into four 16 MiB chunks). Previously, return values from `glClientWaitSync` were ignored, allowing timeouts or wait failures to overwrite buffers still being read by the GPU, and the buffer/fences leaked if the host context outlived the backend. It now strictly requires `GL_ALREADY_SIGNALED` or `GL_CONDITION_SATISFIED` to reuse chunks; on timeout (5-second bounded wait), wait failure, or null fence, it permanently disables persistent ring uploads for that backend and falls back to existing streaming buffers with a single log warning. It also ensures proper destruction of all fences and the 64 MiB persistent buffer upon backend teardown (even if shader compilation fails post-creation), and corrects `dataCapacity` tracking when fallback streaming buffers shrink `dataBuffer`.
* **Measured gain:** **No FPS improvement** (*"No se atribuye a este parche una mejora de FPS"*). It is strictly a stability, correctness, and resource-leak patch.
* **Runtime env vars / switches added or needed:** None added or needed for normal operation. (`GOW_GS_GPU_TEST=1` is an offline test-suite switch to run real-GPU unit tests).
* **Default that must be set to benefit:** None. Automatically active whenever the OpenGL GPU backend upload ring is used.
* **Citations:** 
  * [`docs/RENDER_UPLOAD_RING.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDER_UPLOAD_RING.md#L1-L42), sections `# Protección del ring de uploads OpenGL`, `## Comportamiento`, and `## Regresiones y evidencia` (lines 1–42, 72–80, 100–101).
  * [`docs/ESTADO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/ESTADO.md#L2430-L2457), sections `### Verificación del profiler y protección de uploads (8 de octubre)` (lines 2430–2438) and `### Ring OpenGL validado con la versión conjunta (9 de octubre)` (lines 2440–2457).

---

#### 2. `vu1-zero-factor` (`ps2recomp-vu1-zero-factor.patch`)
* **What it does:** Bypasses slow exact `double` / `long double` arithmetic in FMAC operations when at least one factor is ±0 (a very common case due to zero-filled vectors and matrices):
  1. In `productStickyFlags` (both interpreter and compiled code), an operand of ±0 yields an exact product of zero with the sign of `a XOR b`, determining sticky flags Z and S immediately without `double` calculations.
  2. In the compiled scalar FMAC path (`fmacLaneT`), for MUL/OPMULA and MADD/MSUB/OPMSUB with a zero factor, the float result is already exact (`ACC ∓ 0` is exact in float), so `normalizeFmacExactResult` is skipped and flags are set directly (Z if zero, S from sign bit).
  3. In `commitReadyPipelines`, entries for Q and P pipelines are marked free (`valid = false`) without zeroing all structure fields.
* **Measured gain:** Measured in deterministic VIF1/VU1 frame replay (`vif_port_480s`, no GS rasterization, alternating executables):
  * Compiled VU1: **103.2 ms → 99.0 ms per frame** (**-4.2 ms / ~4.1% gain**, median across 6 rounds of 150 passes).
  * Interpreter VU1: **313.9 ms → 307.0 ms per frame** (**-6.9 ms / ~2.2% gain**, median across 5 rounds of 40 passes).
* **Runtime env vars / switches added or needed:** None.
* **Default that must be set to benefit:** None. Automatically benefits both the interpreter and compiled VU1 pipelines.
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L587-L611), section `## FMAC con un factor cero sin la ruta exacta (9 de octubre)` (lines 587–611).

---

#### 3. `vu1-direct-store` (`ps2recomp-vu1-direct-store.patch`)
* **What it does:** In compiled VU1 code, writes store instructions (SQ, SQI, ISW) immediately to memory on the spot via `vu1c::storeT<DEST>` (writing xyzw all at once or performing a 16-byte read-merge-write with constant lanes), bypassing the 1-cycle latency store queue (`queueStore`) and cycle-by-cycle commit loop. This is semantically identical because compiled execution immediately advances cycles and only one lower instruction executes per pair. The interpreter remains untouched.
* **Measured gain:** Measured in deterministic frame replay (`vif_port_480s`, alternating executables, 6 rounds of 150 passes):
  * Compiled VU1: **99.9 ms → 93.0 ms per frame** (**-6.9 ms / ~6.9% gain**, median).
* **Runtime env vars / switches added or needed:** None.
* **Default that must be set to benefit:** None at runtime. Only applies to compiled VU1 microcode (requires compiled microcode enabled at build time via `GOW_VU1_MICROCODIGO`; does not benefit the fallback interpreter).
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L612-L625), section `### Stores de VU1 compilados escritos en el acto (9 de octubre)` (lines 612–625).

---

#### 4. `vu1-cold-scalar` (`ps2recomp-vu1-cold-scalar.patch`)
* **What it does:** Moves the cold scalar fallback of compiled FMAC operations out of line into non-inlined functions (`fmacScalarT<Upper, DeadFlags>` marked with `VU1C_NOINLINE`), and forces inlining on `vu1c::reverse4` (`VU1_STEP_INLINE`, which MSVC previously compiled as a call). Previously, inlining scalar lanes across all compiled FMACs blew up hot blocks (e.g. block `b3C20_1` was ~74 KB, blowing past the CPU's 32 KB L1 instruction cache). Out-of-line extraction shrank the block to ~6,600 instructions and decreased the runner binary size (`repetir_c.exe` dropped from 55 MB to 36 MB).
* **Measured gain:** Measured in deterministic frame replay (`vif_port_480s`, alternating executables, 6 rounds of 150 passes):
  * Compiled VU1: **92.7 ms → 90.8 ms per frame** (**-1.9 ms / ~2.0% gain**, median).
* **Runtime env vars / switches added or needed:** None.
* **Default that must be set to benefit:** None. Automatically benefits compiled VU1 code.
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L626-L637), section `### Ruta escalar de las FMAC compiladas fuera de línea (9 de octubre)` (lines 626–637).

---

#### 5. `vu1-simd-zero` (`ps2recomp-vu1-simd-zero.patch`)
* **What it does:** Allows the compiled SSE path of MADD, MSUB, and OPMSUB to handle lanes where one factor is zero, instead of rejecting SSE and falling back to the cold scalar path (a frequent case for MADDA.y and MADD.w, such as `ACC + VF29 × VF0.w`). Also accepts lanes where one factor is ±1 (where product is exact): with result exponent 2..0xFD the value does not change and flags match the sign, and if ACC cancels the product it yields exact zero (+0, Z).
* **Measured gain:** Measured in deterministic frame replay (`vif_port_480s`, alternating executables, 6 rounds of 150 passes):
  * Compiled VU1: **90.3 ms → 82.7 ms per frame** (**-7.6 ms / ~8.4% gain**, median). The ±1 factor handling added ~1% of that gain (82.8 → 82.1 ms).
* **Runtime env vars / switches added or needed:** None.
* **Default that must be set to benefit:** None at runtime. Requires compiled VU1 targeting an SSE4.1-capable CPU.
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L638-L653), section `### MADD/MSUB con un factor cero en la ruta SSE (9 de octubre)` (lines 638–653).

---

#### 6. `vu1-xgkick-flush` (`ps2recomp-vu1-xgkick-flush.patch`)
* **What it does:** When flushing pipelines at the end of a VU1 program (`flushPipelines`), if only PATH1 (XGKICK) remains active (no pending Q, P, flags, stores, or register writes): transfers the rest of the packet in a single batch and directly adds the cycles it would have taken (`n` iterations with initial credit `c0` finish at cycle `2n - c0`). This replaces the cycle-by-cycle `advanceOneCycle` / `progressXgkick` loop that previously accounted for 95% of `VU1Interpreter::run`. Packet errors abort at the exact same iteration. Works for both interpreter and compiled code.
* **Measured gain:** Measured in deterministic frame replay (`vif_port_480s`, alternating executables, 6 rounds of 150 passes):
  * Compiled VU1: **82.2 ms → 78.2 ms per frame** (**-4.0 ms / ~4.9% gain**, median).
  * Interpreter VU1: Applies to the interpreter as well, but specific standalone millisecond gain for interpreter was **unverified** / not reported in this section.
* **Runtime env vars / switches added or needed:** None.
* **Default that must be set to benefit:** None.
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L654-L667), section `### PATH1 pendiente al terminar un programa, de una vez (9 de octubre)` (lines 654–667).

---

#### 7. `gs-shared-host` (`ps2recomp-gs-shared-host.patch`, Patch #75)
* **What it does:** Shares OpenGL textures directly between the GS GPU producer context and the raylib host consumer via Windows WGL context sharing, eliminating the per-frame CPU pixel readback from VRAM to RAM. The shared latch queues presentation asynchronously without blocking host Flush/Sync. Diagnostic captures use on-demand readback (`forceReadback`), composing current VRAM with the CRTC parameters from the last latch. If shared context initialization fails, it falls back to the RAM presentation path.
* **Measured gain:** **No stable FPS gain demonstrated** (*"No se atribuye a esta ruta una mejora de FPS"* / *"no demuestra una mejora estable de FPS"*):
  * In a 4-run clean benchmark (180 s each, alternating RAM / Shared):
    * RAM presentation: **6.848 FPS** (Run 1: 6.756 FPS, Run 4: 6.932 FPS).
    * Shared presentation: **6.908 FPS** (Run 2: 6.918 FPS, Run 3: 6.898 FPS).
    * Difference: **+0.88%** (+0.06 FPS), which is smaller than the ~2.6% variation between the two RAM runs.
* **Runtime env vars / switches added or needed:**
  * Exact env var: **`PS2X_GS_DIRECT_PRESENT=1`** (requests WGL shared context; requires GPU backend `PS2X_GS_GPU=1`).
  * Script switch: `.\scripts\probar_rendimiento.ps1 -Renderer opengl -Presentacion compartida`.
  * Runtime verification: The log must state `[gs-present] host mode: shared texture (opt-in)` to confirm it was effectively activated (otherwise it logs `host mode: ram fallback`).
* **Default that must be set to benefit:**
  * **Must explicitly set `PS2X_GS_DIRECT_PRESENT=1`** (or `-Presentacion compartida`), because the default is **`0` / `-Presentacion ram`**.
  * Must run with the OpenGL GPU backend on Windows (`-Renderer opengl`); running with CPU renderer automatically falls back to RAM.
* **Citations:** 
  * [`docs/RENDER_PRESENTACION_COMPARTIDA.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDER_PRESENTACION_COMPARTIDA.md#L1-L28), sections `# Presentación compartida de OpenGL`, `## Selección y propiedad`, `## Cola y capturas`, and `## Regresiones y evidencia` (lines 1–28, 31–42, 64–65).
  * [`docs/RENDIMIENTO_PRESENTACION.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO_PRESENTACION.md#L48-L61), sections `# Comparación de presentación RAM y compartida`, `## Condiciones`, and `## Resultado` (lines 1–26, 48–61).
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L24-L51), section `## Comparar presentación RAM y compartida` (lines 24–51).
  * [`docs/ESTADO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/ESTADO.md#L2458-L2503), sections `### 2026-10-09: presentación compartida sobre las PR #31 y #32 de Opus` (lines 2458–2488) and `### 2026-10-09 — Comparación limpia de presentación` (lines 2489–2503).

---

#### 8. `vu1-generator-dir` (`ps2recomp-vu1-generator-dir.patch`)
* **What it does:** Modifies `ps2xRuntime/CMakeLists.txt` so that CMake passes the microcode directory path (`${PS2X_VU1_MICROCODE_DIR}`) to `ps2x_vu1_generator` instead of passing individual file paths (`${PS2X_VU1_IMAGES}`). This was necessary because capturing microcode through the "Final Battle" grew the dump from 501 to 2,302 micromemories, and passing 2,302 paths exceeded the Windows 32,767 (32K) command-line character limit.
* **Measured gain:** **0 ms direct execution speedup** (the generated C++ for identical inputs is byte-for-byte identical). Functionally, it allows compiling all 2,302 micromemories without crashing the Windows build, preventing the "Final Battle" from falling back to the interpreter (*"Con lo capturado hasta ahora, la Batalla final no pasa por el intérprete"*).
* **Runtime env vars / switches added or needed:**
  * Build-time env var: **`GOW_VU1_MICROCODIGO=<folder>`** (which CMake passes via `-DPS2X_VU1_MICROCODE_DIR`).
  * Runtime env vars: None added. (At runtime, `GOW_VU1_SIN_COMPILAR=1` can disable compiled code, but that is pre-existing).
* **Default that must be set to benefit:**
  * Build-time: Must set `GOW_VU1_MICROCODIGO=<folder>` before running `compilar.ps1` / CMake to build the compiled VU1 runner; otherwise the build falls back to the interpreter.
* **Citations:** 
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L668-L680), section `### Más microcódigo capturado: Batalla final (9 de octubre)` (lines 668–680).
  * [`scripts/compilar.ps1`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/scripts/compilar.ps1#L204), lines 204, 228–230.

---

### 2. Latest Measured FPS in Loaded Gameplay and Configuration Used

* **Latest Measured Game FPS:**
  * **6.848 FPS** (RAM presentation, weighted average across Runs 1 & 4: 6.756 FPS and 6.932 FPS).
  * **6.908 FPS** (Shared presentation, weighted average across Runs 2 & 3: 6.918 FPS and 6.898 FPS).
  * Host refresh rate remained steady at **~59.9 Hz** in both modes.
  * Measured as entries per second into `vid::Flip` (`guest_flip_hz`) during loaded gameplay in state 11 (`pending=0`, `levelReady=1`), sampled between seconds 100 and 170 across 180-second runs using `tools/rendimiento/resumir.py --partida --desde 100 --hasta 170`.
  *(Note: An earlier Oct 8 measurement reported 7.58 FPS for a 64-patch build and 9.6–10.0 FPS during seconds 10–35, but the Oct 9 4-run evaluation is the latest loaded gameplay benchmark).*
* **Configuration Used:**
  * **Renderer:** OpenGL (`-Renderer opengl`).
  * **VU1 Compiled:** Yes; built with **501 private local micromemories** generated and linked (`GOW_VU1_MICROCODIGO`).
  * **Runtime Environment Variables and Switches:**
    * `GOW_FAST_BOOT=1`
    * `GOW_SKIP_FMV=1`
    * `GOW_PAD_TEST_NO_CAPTURE=1` (scripted gamepad input sequence, periodic frame captures disabled)
    * `-SoloCuadros` switch (frames only; internal profiling timers, GPU profiler, draw discarding `PS2X_GS_DISCARD_DRAWS`, and detailed GS diagnostics disabled)
    * `PS2X_GS_DIRECT_PRESENT=1` for shared presentation passes; `0` for RAM presentation passes.
    * Initial state: Private memory card absent at start of each run; private shader cache reused.
  * **Hardware & OS Platform:**
    * CPU: AMD Ryzen 7 5800XT (8 cores, 16 threads)
    * GPU: AMD Radeon RX 5700 XT (Windows driver `32.0.21045.5002`)
    * Executable: Official build of commit `343404f` (75 patches, 70,469,120 bytes, SHA-256 `33C012EF1BB0DC01C325027480893CDB427F406745C686FC4D8296F1BFFFE5A5`, LTCG disabled).
* **Citations:**
  * [`docs/RENDIMIENTO_PRESENTACION.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO_PRESENTACION.md#L1-L61), sections `# Comparación de presentación RAM y compartida`, `## Condiciones`, `## Selección y límites`, and `## Resultado` (lines 1–61).
  * [`docs/RENDIMIENTO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO.md#L47-L51), section `## Comparar presentación RAM y compartida` (lines 47–51).
  * [`docs/ESTADO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/ESTADO.md#L2489-L2503), section `### 2026-10-09 — Comparación limpia de presentación` (lines 2489–2503).

---

### 3. What They Say the Next Bottleneck Is

* **Identified Bottleneck:** **GS / GPU rasterization time**, which takes approximately **50–56 ms per presented frame** (compared to only 0.02–0.03 ms for composition).
* **Details of the Dominant Bottleneck:**
  * The dominant workload targets **FBP 0** with flags **`0x4b`** (combining color interpolation `FIip`, texturing `FTme`, alpha blending `FAbe`, and bilinear filtering `FLinear`).
  * In the active hardware rasterization path (`expanded triangles`), the fragment shader checks coverage before shading, but fragments without coverage still execute through the ordered critical section of the invocation interlock (`beginInvocationInterlockARB` / `endInvocationInterlockARB`).
* **Proposed Next Steps / Experiments:**
  * Testing an early `discard` before the invocation interlock (permitted by the `ARB_fragment_shader_interlock` specification, which allows an early `discard` but forbids putting interlock inside an `if` block or returning before it).
  * Investigating texture caching and fragment discard strategies while preserving CPU rasterization parity (subpixel coverage, bilinear filtering, texture/target aliasing, and alpha blending order).
* **Citations:**
  * [`docs/RENDER_PRESENTACION_COMPARTIDA.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDER_PRESENTACION_COMPARTIDA.md#L109-L130), section `## Siguiente investigación: rasterizado` (lines 109–130).
  * [`docs/RENDIMIENTO_PRESENTACION.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO_PRESENTACION.md#L69-L73), section `## Resultado` (lines 69–73).
  * [`docs/ESTADO.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp-new/god-of-war-recomp-main/docs/ESTADO.md#L2504-L2510), section `### 2026-10-09 — Comparación limpia de presentación` (lines 2504–2510).
