# Task: low-level card routines of MC2_D.IRX (read-only analysis of disassembly)

Use read_file only (no shell commands). Give instruction addresses for every claim; mark guesses UNVERIFIED.

Read: ../docs/memory-card.md, results/mc2-protocol.md (your earlier analysis: dispatch 0x1f74, cmd handlers 1/8 0x2190,
2 0x22e8, 3 0x2904, 4 0x2950, 5 0x29e4, 6 0x29cc, 7 0x2a0c, 0xC 0x2a98, 0xD 0x29a8), and requests/dis/MC2_D.txt
(`; -> lib#N` = imported call: sio2man / sio2d / secrman / dbcman ...).

Goal: we will replace MC2_D's *lowest* routines that talk to the card hardware (through sio2man/sio2d transfers)
with native C++ that reads/writes a raw card image file (page = 512 data + 16 spare bytes). Everything above them stays.

For each command handler, follow the calls down to the routines that issue sio2man/sio2d transfers and report, per
low-level routine (address):
1. What it does on the card (read page(s) with/without spare, write page(s), erase block, read card ID/specs,
   card-present/changed check, authentication via secrman, set-termination-code, ...).
2. Its arguments ($a0-$a3 and stack args: port/slot, page number, page count, buffer addresses (IOP RAM), flags) and
   return value meaning ($v0: 0 = ok? error codes?), including any global variables it reads/writes (absolute addresses).
3. Where read data lands (buffer address, layout of data vs spare bytes, e.g. 0x4498).
4. Which routine is the single best place to cut (highest routine that only does card I/O, called by several
   handlers), and whether the authentication (secrman) path can be skipped by returning success at some address.
Also: what cmd 4's port-state check 0x544 reads (is it a global set by an interrupt/poll?) and what values mean
"card present, unchanged" vs "changed" vs "none".
Output under 140 lines: one section per routine, then a recommended cut list (address, C-like signature, behaviour).
