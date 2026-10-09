# Task: review a new threading change (second opinion; read-only)

Context: results/vif1-shared-state.md (your earlier map). We implemented an opt-in worker thread
(env PS2X_GS_WORKER=1) that runs VIF1 + GIF PATH3 DMA processing (VIF1 -> VU1 -> GS frontend) off the EE thread.
Code (all under ../ext/PS2Recomp-kl/ps2xRuntime):
- include/runtime/ps2_memory.h: search "PS2X_GS_WORKER", GsWorkerItem, GsWorkerJob, syncGsWorkerFor,
  syncGsWorkerIfOtherThread, m_gsWorker*.
- src/lib/ps2_memory.cpp: PS2Memory::PS2Memory / ~PS2Memory; processPendingTransfers (collectForWorker lambda,
  enqueue at the end); appendTransferBytes, runGsWorkerJob, gsWorkerLoop, syncGsWorker, stopGsWorker,
  needsGsWorkerSync; every read8..read128 / write8..write128 / readIORegister / writeIORegister now starts with
  syncGsWorkerFor(address); submitGifPacket, processGIFPacket(uint32_t,uint32_t), flushMaskedPath3Packets,
  tryProcessNativeGifImageUploadChain, tryProcessNativeGifPackedChain start with syncGsWorkerIfOtherThread().
- src/lib/ps2_vif1_interpreter.cpp: processVIF1Data(const uint8_t*, uint32_t) starts with syncGsWorkerIfOtherThread().
- src/lib/ps2_runtime.cpp: syncCoreSubsystems (eeContextForVu1, setVu1StopEnables, publishVu1Stop,
  setFbrstProvider); ~PS2Runtime and shutdownHostGraphics call m_memory.stopGsWorker().
- src/lib/Kernel/Stubs/GS.cpp and Kernel/Stubs/Helpers/Support.h: syncGsWorker() before direct gs() calls.

Find concrete bugs only (each with file:line and a failure scenario):
1. Deadlocks (e.g. worker calling something that waits for itself; lock order with GSThreadedBackend mutexes,
   m_completedDmacMutex, GS m_stateMutex; sync called while holding a lock the worker needs).
2. Remaining data races: state the worker writes that another thread reads/writes without syncGsWorker first
   (check EeScheduler vsync/pollGsInterrupt, gs_regs fields, m_path3Masked, m_seenGifCopy, VU1 code generation,
   anything in ps2_runtime.cpp or Kernel/ that calls m_gs / runtime->gs() / memory VU1 pointers directly).
3. Ordering changes vs the old inline code (GIF vs VIF1 order, arbiter drain, partial-quadword handling in
   appendTransferBytes vs the old loops, MSKPATH3 interplay).
4. Shutdown/lifetime problems (thread started in PS2Memory ctor; m_gsWorkerId set after thread start — race?).
Do NOT suggest style changes. Mark anything not confirmed in code as **unverified**. Local files only.
