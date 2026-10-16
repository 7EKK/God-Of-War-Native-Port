# agent/athn01d: ship deck ATHN01D toward 60 fps (2026-10-16)

Worktree ext/PS2Recomp-kl-athn, branch agent/athn01d from gow-usa-kl 0ddab33 (merged28), runner copied from
ext/PS2Recomp-kl (merged28), build out/build-athn (Release, /MAP). Base exe kept in port/athn01d/bin_base.

## Profile (port/athn01d/prof_base*.txt, prof_dll.txt; tools/sample_prof.py --dll-exports)
Frame = worker job (VIF1 + VU1 of one ~3 MB chain) + EE gap; job 17-20 ms, gap 3-5 ms. Worker thread:
- native VU1 blocks 32 %, runNative (dispatch) 15 %, commitReadyPipelines 3.4 %, resetScheduler 2.4 %;
- **heap: RtlAllocateHeap 6.8 % + RtlFreeHeap 0.6 %** (was hidden inside "<ntdll.dll>"), memcpy/memset 3-4 %;
- waits 13 % (between jobs, not inside them).
Sources of the allocations: GifArbiter::submit copied every XGKICK packet into a new vector before the stage copied it
again; the stage queue was a std::deque (MSVC: one heap block per 32-byte Packet). EE gap: 1.5 ms of it inside 0x13F730
= sendDmaChain gathering the 3 MB chain into a fresh vector (~20 reallocations, new pages each time).

## Changes (commit on agent/athn01d)
- GifArbiter::submitAndDrain: with nothing queued the packet goes to the callback in place (every XGKICK).
- gs_stage queue/batch: std::vector (capacity kept across swaps).
- DMA buffer pool in ps2_memory.cpp (takeDmaBuffer / returnDmaBuffer, max 4): sendDma/sendDmaChain reuse the buffers the
  worker returns after a job.
- PS2X_GS_WORKER_TRACE: `gs-irq` line when the GS stage raises FINISH/SIGNAL (stage tail vs EE wake in the gap).
- tools/levels.py --idle (stand still after the warp: repeatable frames for A/B); tools/sample_prof.py --dll-exports.

## Measurements (port/athn01d/ab/, ab.sh; interleaved base/new; another fork's tests ran part of the time)
| run | job ms | gap ms | fps min / mean |
|---|---|---|---|
| idle base x2 | 17.09 / 17.17 | 2.79 / 2.80 | 49.0 / 51.3, 48.8 / 50.9 |
| idle new x2 | 15.50 / 15.11 | 1.55 / 1.59 | 58.8 / 59.6, 58.6 / 59.6 |
| moving base x3 | 18.31 / 17.17 / 17.08 | 2.8-3.1 | 46.4 / 49.6, 45.8 / 50.1, 49.4 / 52.2 |
| moving new x3 | 14.79 / 13.41 / 12.05 | 1.8-4.6 | 59.4 / 59.8, 58.4 / 59.5, 53.8 / 58.9 |
Moving runs vary with the camera (job 2.4-3.3 MB); an earlier moving set (B_r*) ran while other tests loaded the
machine and showed no gain: only interleaved runs on an idle machine count. Gap after the change (idle): stage tail
(job-done -> gs-irq) ~0.55 ms, EE wake ~0.3 ms, EE list build ~1.5 ms -> ~0.7 ms less in 0x13F730.

## Gate
port/athn01d/gate (zones 0-4, baseline port/merged28/gate), port/athn01d/savetest.

## Left
- Worker: dispatch loop (runNative ~18 % now) and per-program resetScheduler/commitReadyPipelines; block chaining in
  the generator is the next real lever (docs/performance.md).
- GS stage thread is ~70 % busy now (vertexKick 20 %): next bottleneck once the job shrinks further.
- EE spins in 0x17A2A0 for the whole job (a full core; matters on the Switch's 3 cores).
