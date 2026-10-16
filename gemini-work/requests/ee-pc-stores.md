# Task: can the recompiled EE code drop its per-instruction `ctx->pc` stores?

Use read_file only (no shell commands). Read-only analysis; do not edit anything.

Context: the PS2 EE (R5900) code of God of War is statically recompiled to C++. Every generated function
(example: ext/PS2Recomp-kl-ee/ps2xRuntime/src/runner/sub_0017A0E0_0x17a0e0.cpp) begins with a `switch (ctx->pc)`
that can jump to a label before EVERY instruction, and stores `ctx->pc = 0x...u;` before every instruction.
We want to know which of these are needed.

Files to read:
- ext/PS2Recomp-kl-ee/ps2xRecomp/src/lib/function_emitter.cpp
- ext/PS2Recomp-kl-ee/ps2xRecomp/src/lib/control_flow_emitter.cpp
- ext/PS2Recomp-kl-ee/ps2xRecomp/src/lib/code_generator.cpp
- ext/PS2Recomp-kl-ee/ps2xRuntime/src/runner/sub_0017A0E0_0x17a0e0.cpp (generated example)
- ext/PS2Recomp-kl-ee/ps2xRuntime/src/runner/sub_0017A2A0_0x17a2a0.cpp (generated example with a loop)

Questions (answer each with file:line evidence):
1. Where exactly are (a) the per-instruction `ctx->pc = ...` stores, (b) the per-instruction labels, (c) the
   entry `switch (ctx->pc)` emitted? Is there an existing option/flag to restrict labels to real branch targets
   / resume points?
2. After a JAL / dispatchGuestBranch returns false, or `eeCheckpointDue()` returns true, the function returns and
   is later re-entered through the switch with ctx->pc = some address. List the kinds of addresses that can be
   resume points (branch targets, return addresses after calls, checkpoint sites, delay slots, ...).
3. Which emitted statements other than the per-instruction store set ctx->pc (before calls, syscalls, checkpoints)?

Output: a concise markdown list (max 60 lines) in this format: `finding — file:line`. Mark anything you are not
sure about as UNVERIFIED.
