# Task: extract findings from ONE file

Read-only. Use read_file only (no shell commands). Read ONLY ../ref/other-recomp-git/docs/ESTADO.md
(Spanish). Focus on the sections added most recently (palette / joints / skinning / camera / PCSX2 EE reference /
VU1 thread).

We investigate why Kratos' torso is twisted ~180° while walking in a recompiled port of the same game (same
code base). Our EE code, VU1 and animation layers already match PCSX2.

Output (max 30 lines, English): every concrete finding about joint matrices, the model palette, skinning
buffers/registers, where his port differs from PCSX2 (addresses, functions, values), and his stated next step.
Quote addresses/function names exactly. Mark unsure items UNVERIFIED.
