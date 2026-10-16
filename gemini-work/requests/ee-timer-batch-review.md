# Task: review a change for exactness (EE timers batched)

Use read_file only (no shell commands). Read-only; do not edit anything.

File: ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp (functions resetEeTimers, flushEeTimers,
advanceEeTimers, stepEeTimers, cyclesUntilNextEeTimerInterrupt, writeIORegister and readIORegister timer branches)
and ext/PS2Recomp-kl-ee/ps2xRuntime/include/runtime/ps2_memory.h (m_eeTimerPendingCycles, m_eeTimerBudget).

Before the change, advanceEeTimers(c) ran the per-timer math (now stepEeTimers) on every call (c is usually 32
EE cycles, millions of calls per second). Now it accumulates c into m_eeTimerPendingCycles and runs stepEeTimers
only when pending >= m_eeTimerBudget (= cyclesUntilNextEeTimerInterrupt() computed after the previous step);
timer register reads/writes flush pending cycles first, and writes force a budget recompute.

Claim: the guest observes exactly the same timer counts, mode flags and interrupt masks (returned at the same
advanceEeTimers call) as before. Check:
1. Is stepEeTimers(a+b) identical to stepEeTimers(a); stepEeTimers(b) when no interrupt event lies inside a
   (including ZRET wrap, compare==0, overflow, the clockRemainder carry, all four clock sources)?
2. Is cyclesUntilNextEeTimerInterrupt exact (never larger than the true number of cycles to the next raised
   interrupt)? Any case where an interrupt bit could be raised but the function returns "no event" (max)?
3. Any other code path that reads or writes m_eeTimers (or timer state) without flushing first?
4. Invariant pending < budget: can flushEeTimers ever cross an event and drop an interrupt?

Output: max 40 lines, each finding with file:line; say clearly "no problem found" for each point if so. Mark
anything unsure as UNVERIFIED.
