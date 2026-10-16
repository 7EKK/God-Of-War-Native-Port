# Task: verify a C++ re-implementation of PCSX2 pnach "extended" codes (read-only review)

Files to read (only these):
1. ../ref/pcsx2-src/pcsx2/Patch.cpp — function Patch::handle_extended_t (around lines 1225-1700) and
   Patch::ApplyPatches / ApplyVsyncPatches (around lines 1100-1165).
2. requests/widescreen-gow_overrides_draft.cpp.txt — only the function `applyGowPnachExtended` and the table
   `kGowWidescreenCodes` (search for them; ignore everything else).

Questions (answer each with YES/NO + one or two sentences, quoting the Patch.cpp line numbers you used):
A. Is the E-code to D-code conversion (Ezyyvvvv taaaaaaa -> Daaaaaaa yytzvvvv) identical to PCSX2's?
B. For cond 0..7 and type 0 (16-bit) / 1 (8-bit), does `skipLines` match the PCSX2 condition that sets skip_count?
C. Is skip_count handling identical (yy == 0 means skip 1; while skipping, every following extended line is just
   counted, including nested E-codes)?
D. Are 0aaaaaaa / 1aaaaaaa / 2aaaaaaa writes the same sizes/values as PCSX2?
E. Given the table, with RAM u16[0x29CC90]==1: which writes happen for u16[0x29CC50]==0 and for ==1? List them.
F. Is ExtendedState reset per pass (per ApplyPatches call) in PCSX2, so that skip state does not leak across vsyncs?
Mark anything you could not verify as UNVERIFIED. Do not suggest code; just answer A-F.
