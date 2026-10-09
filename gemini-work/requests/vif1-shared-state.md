# Task: map shared state of the VIF1 -> VU1 -> GS path (for moving it to a worker thread)

Code root: ../ext/PS2Recomp-kl/ps2xRuntime (C++). Read only.
Goal: we want PS2Memory::processPendingTransfers' VIF1 and GIF(PATH3) processing to run on a dedicated worker
thread (like PCSX2 MTVU), while the EE (recompiled game code + EeScheduler) keeps running. We need the exact list
of state that this path touches which the EE thread (or host/presentation thread) also touches.

Start at:
- src/lib/ps2_memory.cpp: processPendingTransfers, processVIF1Data (all overloads), submitGifPacket,
  the DMA CHCR write handler that builds m_pendingVif1Transfers/m_pendingGifTransfers (chainData),
  enqueueTransfer, completeDmacChannel, queueCompletedDmacCause.
- src/lib/ps2_vif1_interpreter.cpp (if VIF1 is processed there).
- src/lib/ps2_runtime.cpp: syncCoreSubsystems (VU1 MSCAL/MSCNT callbacks), the GIF arbiter setup.
- VU1Interpreter::execute/resume and XGKICK -> GS (src/lib/vu/*), GS::processGIFPacket (src/lib/gs/gs_frontend.cpp).

Report:
1. Every member/global written or read on this path (name, file:line, type e.g. std::unordered_map, atomic),
   and whether EE-side code (Load*/Store* MMIO handlers in ps2_memory.cpp / ps2_runtime.cpp, syscalls,
   EeScheduler, interrupt delivery) or the host thread also reads/writes it (file:line).
2. Every place this path raises interrupts or signals the EE (INTC, DMAC D_STAT, VIF IRQ/stall, VU1 T/D bits,
   GS SIGNAL/FINISH/LABEL, vu0_vpu_stat) and how the EE observes it.
3. Every place the path reads EE RAM or scratchpad lazily (i.e. not from a snapshot taken at DMA kick time).
4. MMIO addresses whose EE access would need to wait for the worker to drain (VIF1 regs, GS privileged regs,
   VU1 memory, GIF regs, DMA channel regs) and where those accesses are handled (file:line).
5. GS frontend (class GS) entry points called from the host/presentation thread (file:line).
Be precise; mark anything you did not confirm in code as **unverified**. Use only local files.
