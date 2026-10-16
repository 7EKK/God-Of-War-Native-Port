# Task: map the VU1 XGKICK -> GIF -> GS path cost (read-only)
Use read_file only (no shell commands).

File: ../ext/PS2Recomp-kl-vu/ps2xRuntime/src/lib/vu/ps2_vu1_core.cpp (large). Also headers it needs in
../ext/PS2Recomp-kl-vu/ps2xRuntime/include/runtime/ (ps2_vu1.h).

Goal: VU1NativeAccess::xgkick (in ps2_vu1_native.inl, same folder) calls VU1Interpreter::progressXgkick(). Describe, for one call:
1. Every function called (transitively, inside ps2_vu1_core.cpp and whatever GS/GIF entry it calls next: name the function and file:line).
2. Per-qword / per-packet work: copies of data, std::vector allocations/resizes, maps/hash lookups, locks, virtual calls, logging/env checks.
3. Anything in runNative() or VU1Interpreter::run() that runs per MSCAL (per program run) and looks expensive (allocations, memsets of big arrays, copies of VU1State, std::getenv).
Output: a numbered list with file:line for every claim, max 60 lines. Mark anything unverified.
