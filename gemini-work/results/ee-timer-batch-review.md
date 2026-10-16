### Task
Review exactness of EE timer batching in [`ps2_memory.cpp`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp) and [`ps2_memory.h`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/include/runtime/ps2_memory.h).

### Findings

1. **stepEeTimers(a+b) vs stepEeTimers(a); stepEeTimers(b)**: **No problem found**.
   - [`ps2_memory.cpp:479-484`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L479-L484): Clock tick math `ticks = wholeSeconds * clockHz + scaled / kEeClockHz` and `clockRemainder = scaled % kEeClockHz` algebraically satisfy `ticks(a+b) == ticks(a) + ticks(b)` and `remainder(a+b) == remainder_b` for all 4 clock sources.
   - [`ps2_memory.cpp:498-517`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L498-L517): When no interrupt event lies inside `a`, distances shift by `ticks_a`. Under both ZRET mode (`lines 503-506`, including `compare == 0`) and normal mode (`line 516`), `compareReached`, `overflowReached`, mode flags (`Equf`, `Ovff`), and final `timer.count` match identically.

2. **cyclesUntilNextEeTimerInterrupt exactness**: **No problem found**.
   - [`ps2_memory.cpp:543-561`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L543-L561): Event distances use [`ticksUntilMatch`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L194) and identical conditions to [`stepEeTimers`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L468), including `overflowCanOccur` under ZRET (`overflowDistance <= compareDistance`).
   - [`ps2_memory.cpp:567-570`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L567-L570): Cycle count `(numerator + clockHz - 1u) / clockHz` with `numerator = eventTicks * kEeClockHz - clockRemainder` is the exact ceiling integer division of required EE cycles. It is never larger than the true cycles to the next interrupt.
   - No case exists where an interrupt bit could be raised but the function returns `max`.

3. **Timer state access paths without flushing**: **No problem found**.
   - [`ps2_memory.h:502-510`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/include/runtime/ps2_memory.h#L502-L510): `m_eeTimers`, `m_eeTimerPendingCycles`, and `m_eeTimerBudget` are private to `PS2Memory` with no friend classes.
   - [`ps2_memory.cpp:176-192, 1195-1197, 2813-2815`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L1195-L1197): All guest accesses to T0–T3 registers (COUNT, MODE, COMPARE, HOLD) decode via `decodeEeTimerRegister` in [`writeIORegister`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L1190) and [`readIORegister`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L2808), both invoking [`flushEeTimers`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L426) before access.
   - No other code path accesses `m_eeTimers` or timer state without flushing.

4. **Invariant pending < budget & flushEeTimers**: **No problem found**.
   - [`ps2_memory.cpp:426-435`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L426-L435): [`flushEeTimers`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L426) discards the result of [`stepEeTimers`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L468).
   - [`ps2_memory.cpp:455-465`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L455-L465): Invariant `pending < budget` holds whenever guest code runs because [`advanceEeTimers`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L437) steps timers and resets `pending = 0` whenever `pending >= budget`.
   - Because `budget` is exact (Finding 2), any `pending < budget` cannot reach any enabled interrupt event; `stepEeTimers` in `flushEeTimers` always returns 0 and drops no interrupt.
   - [`ps2_memory.cpp:1197-1198`](file:///C:/Users/lovas/Claude/God%20Of%20War/ext/PS2Recomp-kl-ee/ps2xRuntime/src/lib/ps2_memory.cpp#L1197-L1198): Register writes flush before setting `m_eeTimerBudget = 0`, forcing an immediate budget recompute on the next advance.

Confidence: High.
