### 1. PCSX2 microVU: Avoiding Per-Instruction Pipeline Bookkeeping

#### Block-Level Analysis of Stalls and Host Execution
* **Two-Pass Compilation Architecture** ([`microVU_Compile.inl:692-900`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L692-L900)):
  * **Pass 1 (Static Pipeline Simulation)**: At compile time, `microVU` steps through instructions sequentially ([`microVU_Compile.inl:705-854`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L705-L854)). It simulates register read/write readiness using cycle counters stored in `regCycleInfo` ([`microVU_IR.h:8-14`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_IR.h#L8-L14)) for each VF subcomponent ($x, y, z, w$), VI register, Q, P, and XGKICK. Any latency collision computes a static stall: `mVUstall = std::max(mVUstall, ...)` ([`microVU_Analyze.inl:15-135`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Analyze.inl#L15-L135)). The block's cumulative cycle counter is incremented statically (`mVUincCycles(mVU, 1 + mVUstall)` in [`microVU_Compile.inl:331-380`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L331-L380)).
  * **Pass 2 (Host Code Emission)**: The generated host code contains **no runtime cycle decrement loops, no pipeline queues, and no latency bookkeeping between instructions**. It emits raw x86 SIMD/ALU instructions directly.
  * **Block-Level Cycle Budget Check** ([`microVU_Compile.inl:480-498`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L480-L498)): A single check is emitted at the head of each block:
    ```cpp
    xMOV(eax, ptr32[&mVU.cycles]);
    xSUB(eax, 1);
    xSUB(ptr32[&mVU.cycles], mVUcycles);
    ```
    If cycles remain, the entire block runs without checking timers or pipelines.

#### Flag Instances (Status, MAC, Clip)
* **4-Instance Ring Buffer** ([`microVU.h:84-86`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU.h#L84-L86)):
  Because VU FMAC operations have a 4-cycle pipeline latency before flags take effect, `microVU` maintains 4 physical instances of each flag structure: `statFlag[4]`, `macFlag[4]`, `clipFlag[4]`.
* **Cycle-Mapped Writes & Reads** ([`microVU_Flags.inl:63-75, 194-205`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L63-L75)):
  * Writes cycle through instances using `write = (lastWrite + 1) & 3` ([`microVU_Flags.inl:222-236`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L222-L236)).
  * Reads locate the active instance via `findFlagInst(fFlag, cycles)`, which statically maps to the instance completed on or before that cycle ([`microVU_Flags.inl:63-75`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L63-L75)).
* **Block Linking & Normalization** ([`microVU_Flags.inl:255-337`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L255-L337)):
  At block exit/entry, `mVUsetupFlags` normalizes the flag instances into standard indices using SSE shuffles (`xSHUF.PS` on `xmmT1`/`xmmT2` for MAC and Clip, and register moves for Status).

#### Q and P Pipelines
* **Double Buffering (Ping-Pong Instances)** ([`microVU_Compile.inl:306-307, 361-369`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L306-L307)):
  Q (FDIV/FSQRT) and P (EFU) are tracked via binary instance indices (`p ^= 1`, `q ^= 1`). Multi-cycle latency (e.g. 7 cycles for DIV) is decremented statically in Pass 1. Instructions depending on Q or P stall compile-time simulation until `!mVUregs.q` or `!mVUregs.p`, so emitted host instructions access the active Q/P register directly without runtime status polling.

#### XGKICK Timing
* **Memory-Write Hazard Tracking** ([`microVU_Compile.inl:778-795`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L778-L795)):
  `xgkickcycles` tracks elapsed cycles between an XGKICK instruction and subsequent VU data memory writes (SQ/SQD/SQI). If memory is written while an XGKICK DMA buffer could still be reading (`mVUlow.kickcycles`), `mVU_XGKICK_SYNC` ([`microVU_Compile.inl:897`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L897)) synchronizes the transfer.

#### Minimum State a Block-Level Static VU1 Compiler Must Track
* Consolidated in `microRegInfo` (96 bytes, [`microVU_IR.h:22-61`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_IR.h#L22-L61)):
  1. `VI[16]`: Remaining stall cycles per integer register (16 bytes).
  2. `VF[32]`: Remaining stall cycles per vector component ($x, y, z, w$ packed as 4-bit nibbles in `regCycleInfo`, 16 bytes).
  3. `q`, `p`: Active instance indices and remaining busy cycles.
  4. `xgkick`, `xgkickcycles`: In-flight XGKICK transfer state and cycles before memory writes.
  5. `flagInfo`: Active slot indices for Status, MAC, and Clip flag rings.
  6. `viBackUp`: VI register written in a branch delay slot.
  7. `needExactMatch`: Bitmask specifying which pipeline properties must match across block links.
* **Fast Matching**: `quick64[0]` packs `needExactMatch`, `flagInfo`, `q`, `p`, `xgkick`, `viBackUp`, and `blockType` into a single 64-bit integer, resolving >90% of block lookups in a single integer comparison ([`microVU.h:234-248`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU.h#L234-L248)).

#### What microVU Skips / Optimizes Out
* **Dead / Unused Flags**: `_mVUflagPass` ([`microVU_Flags.inl:359-430`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L359-L430)) scans up to 4 instructions into successor blocks across branches. If flags are overwritten before being read or never consumed, flag updates are disabled (`sFLAG.doFlag = false`).
* **Status Flag Elimination under FSSET**: `mVUstatusFlagOp` ([`microVU_Flags.inl:20-61`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L20-L61)) optimizes out unneeded status flag updates prior to an `FSSET` opcode.
* **Flag Hack (`mVUsFlagHack`)**: When enabled ([`microVU_Flags.inl:206-209`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Flags.inl#L206-L209), [`microVU.h:238-242`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU.h#L238-L242)), status flag instance matching is masked out (`quick & ~0x0C04`), avoiding block divergence.
* **End-of-Block Cycle Clamping**: `mVUoptimizePipeState` ([`microVU_Compile.inl:313-329`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/x86/microVU_Compile.inl#L313-L329)) rounds 1 remaining cycle to 0, since the next block automatically accounts for it on entry.

---

### 2. Running VU1 in Parallel on a Dedicated Host Thread (PCSX2 MTVU)

#### Viability and Mechanism
* **Yes**: PCSX2 executes VU1 on a dedicated host thread via `VU_Thread::ExecuteRingBuffer()` ([`MTVU.h:19-32`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.h#L19-L32), [`MTVU.cpp:128-208`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L128-L208)).

#### Required Synchronization
* **Lock-Free Ring Buffer FIFO** ([`MTVU.h:20-27`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.h#L20-L27), [`MTVU.cpp:138-204`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L138-L204)):
  * 16 MB ring buffer with atomic head/tail indices (`m_ato_read_pos`, `m_ato_write_pos`) on distinct cache lines.
  * Commands enqueued by EE: `MTVU_VU_EXECUTE`, `MTVU_VIF_UNPACK`, `MTVU_VU_WRITE_MICRO`, `MTVU_VU_WRITE_DATA`, `MTVU_VU_WRITE_VIREGS`, `MTVU_VU_WRITE_VFREGS`, `MTVU_VIF_WRITE_COL`, `MTVU_VIF_WRITE_ROW`.
  * Backpressure: `WaitOnSize` ([`MTVU.cpp:212-235`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L212-L235)) yields or sleeps when buffer capacity is exceeded.
* **EE Synchronization Points (`WaitVU`)** ([`MTVU.cpp:435-439`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L435-L439)):
  * Occurs when the EE must read VU1 state or when VIF1 executes synchronizing commands (`FLUSHE`, `FLUSH`, `FLUSHA`, or polling `VPU_STAT.VEW`).
* **XGKICK / PATH 1**:
  * VU1 pushes XGKICK packets directly to the GS backend via `FinishGSPacketMTVU()` and signals `semaXGkick.Post()` ([`MTVU.cpp:154-155`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L154-L155)), completely bypassing the EE thread.
* **Interrupts & Register Access**:
  * Unlike VU0 (which has EE COP2 macro-mode instructions `CFC2`/`CTC2`), **VU1 has no direct COP2 connection on the EE**. The EE communicates with VU1 exclusively through VIF1 and DMA Channel 1.
  * GS Signal/Finish/Label and VU T-bit/E-bit interrupts are accumulated in `std::atomic<u32> mtvuInterrupts` ([`MTVU.h:47-51`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.h#L47-L51)) and polled by EE via `Get_MTVUChanges()` ([`MTVU.cpp:351-423`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L351-L423)).

#### Relevance for this Recompiled Port
* God of War uses VU1 strictly as a feed-forward geometry and skinning engine (EE $\to$ VIF1 DMA $\to$ VU1 $\to$ XGKICK $\to$ GS).
* Profiling in loaded gameplay ([`status.md:167, 172-173`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/status.md#L167-L173)) shows the single game thread saturated across 5 s: **VU1: 3.07 s (61%)**, **EE: 1.58 s (31%)**, GS: 0.20 s, IOP: 0.15 s ($\approx 250$ ms/frame $\to$ 4 fps).
* Moving VU1 to an asynchronous worker thread overlaps the 3.07 s VU1 workload with the 1.58 s EE workload, providing an immediate **$\sim 1.8\times - 2.0\times$ overall throughput gain** on multi-core PCs.

---

### 3. [SKIPPED]

---

### 4. EE-Side Performance Bottlenecks in Recompiled R5900 C++ & Known Fixes

Source inspection of [`ps2_runtime_macros.h`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h), [`sub_0017A0E0_0x17a0e0.cpp`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp), and [`ps2_runtime.cpp`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/src/lib/ps2_runtime.cpp):

1. **Per-Instruction Memory Access Helpers**:
   * *Issue*: Every load/store macro (`READ32`, `WRITE32`, etc., [`ps2_runtime_macros.h:516-612`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L516-L612)) invokes an immediately evaluated lambda that branches on `PS2Runtime::isSpecialAddress(_addr)`. On the fast path, `FAST_READxx`/`FAST_WRITExx` calls `Ps2FastRangeIsContiguous` ([`ps2_runtime_macros.h:344-514`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L344-L514)), executing boundary checks and `std::memcpy`. Writes additionally call `ps2TraceGuestWrite` on every store ([`ps2_runtime_macros.h:546-612`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L546-L612)).
   * *Fix*: Compile out trace hooks in Release builds. For stack (`$sp + \text{imm}`) and global memory accesses (`$gp + \text{imm}`), emit direct pointer dereferences (`*reinterpret_cast<uint32_t*>(rdram + (addr & 0x01FFFFFF))`), eliminating MMIO checks and `memcpy`.
2. **128-Bit Register Emulation on Scalar Operations**:
   * *Issue*: Context registers `ctx->r[32]` are stored as `__m128i` ([`ps2_runtime.h:64`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime.h#L64)). Every 32-bit scalar write (`SET_GPR_S32`, [`ps2_runtime_macros.h:1002-1010`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L1002-L1010)) calls `Ps2SetGprLow64`, which executes `_mm_move_sd` ([`ps2_runtime_macros.h:972-978`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L972-L978)) to preserve the upper 64 bits. This forces continuous spilling and SIMD register insertion instead of keeping values in native 64-bit host GPRs.
   * *Fix*: Static recompiler register analysis: >90% of R5900 operations in GoW are standard 32/64-bit scalar instructions. Map GPRs to native host `uint64_t` variables inside functions, syncing to `__m128i` only when MMI instructions are encountered or at function boundaries.
3. **Indirect Call Dispatch and Huge Function Switch Prologues**:
   * *Issue*: Every generated function starts with a `switch (ctx->pc)` containing a case label for every 4-byte instruction address ([`sub_0017A0E0_0x17a0e0.cpp:21-83`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp#L21-L83)). Furthermore, **every direct function call (`JAL`)** ([`sub_0017A0E0_0x17a0e0.cpp:161, 207, 286`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp#L161)) is routed through `runtime->dispatchGuestBranch`. In `ps2_runtime.cpp:1463-1536`, this executes an atomic ring push (`g_gowRingPos.fetch_add`), scheduler check (`eeCheckpointDue`), function table lookups (`hasFunction`, `lookupFunction`), and an indirect call, completely defeating C++ compiler inlining.
   * *Fix*: Emit direct C++ calls (`func_140948(rdram, ctx, runtime)`) for direct `JAL` instructions, allowing native call/return optimization and inlining. Remove the switch prologue for functions without mid-body entry points.
4. **Per-Instruction PC Updates**:
   * *Issue*: Every single recompiled instruction updates `ctx->pc` in memory (`ctx->pc = 0x17a0e0u;`, [`sub_0017A0E0_0x17a0e0.cpp:89, 93, 101`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp#L89)), producing heavy store traffic that prevents basic block instruction scheduling.
   * *Fix*: Update `ctx->pc` only before calls, backward edges, and exception/checkpoint points.

---

### Ranked List of the 5 Highest-Impact Changes for this Port

| Rank | Change | Target Area | Expected Gain | Rationale & Source References |
|---|---|---|---|---|
| **1** | **Block-Level Static VU1 Recompiler (microVU-Style)** | VU1 | **$4\times - 6\times$ VU1 speedup** (drops VU1 from $\sim 155$ ms to $25-35$ ms/frame) | VU1 currently consumes 61% of total frame time ([`status.md:167`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/status.md#L167)). The current compiled VU1 prototype retains per-pair latency queues (`commitReadyPipelines`) and huge switch statements ([`RENDIMIENTO.md:158-166, 526`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/other-recomp/god-of-war-recomp-main/docs/RENDIMIENTO.md#L158-L166)). Statically resolving stalls and eliminating dead flags via 4-instance ring buffers removes the pipeline simulation entirely. |
| **2** | **Multi-Threaded VU1 Execution (MTVU Architecture)** | Architecture / Concurrency | **$1.8\times - 2.0\times$ overall FPS gain** | Currently, EE (77 ms) and VU1 (155 ms) run sequentially on a single core ([`status.md:172-173`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/status.md#L172-L173)). Offloading VU1 to a worker thread via lock-free ring buffers ([`MTVU.cpp:128-208`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/MTVU.cpp#L128-L208)) allows the EE and VU1 workloads to execute fully in parallel. |
| **3** | **Direct C++ Calls for JAL & Eliminating `dispatchGuestBranch` on Direct Branches** | EE (Recompiled Code) | **$2\times - 3\times$ EE speedup** (drops EE from $\sim 77$ ms to $25-35$ ms/frame) | Replacing runtime branch dispatch ([`sub_0017A0E0_0x17a0e0.cpp:161`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp#L161), [`ps2_runtime.cpp:1463-1536`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/src/lib/ps2_runtime.cpp#L1463-L1536)) with direct static C++ calls eliminates atomic ring logging, table lookups, and function pointer indirect calls on every single guest call. |
| **4** | **Fast-Path Memory Access (Removing Lambdas, Trace Hooks, and Range Checks)** | EE (Recompiled Code) | **$1.5\times - 2.0\times$ EE speedup** across memory-heavy loops | Removing lambda overhead, `isSpecialAddress` branches, `ps2TraceGuestWrite` logging, and `Ps2FastRangeIsContiguous` checks ([`ps2_runtime_macros.h:516-612`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L516-L612)) in favor of direct masked pointer dereferencing on stack/RAM addresses. |
| **5** | **Native 64-Bit GPR Storage & Switch Prologue Removal** | EE (Recompiled Code) | **$1.3\times - 1.6\times$ EE speedup** | Eliminates `_mm_move_sd` double-precision inserts on 32-bit GPR writes ([`ps2_runtime_macros.h:972-978`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl/ps2xRuntime/include/ps2_runtime_macros.h#L972-L978)) and 40+ case `switch(ctx->pc)` dispatch blocks ([`sub_0017A0E0_0x17a0e0.cpp:21-83`](file:///C:/Users/lovas/Claude/God%20Of%20War/port/output-usa/sub_0017A0E0_0x17a0e0.cpp#L21-L83)), allowing the host compiler to keep variables in native host CPU registers. |
