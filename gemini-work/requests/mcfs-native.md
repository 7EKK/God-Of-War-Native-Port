# Task: facts for a native PS2 memory-card service (read-only research)

Use read_file only (no shell commands). Mark anything you could not verify in the files as UNVERIFIED.

Context: a native PC port of God of War (PS2). Today the game's memory card works through the PS2 hardware path:
EE libmc2 -> IOP RPC -> MC2_D.IRX (the game's own module) -> SIO2MAN -> an SIO2 chip model -> a raw PCSX2-format card
image (8 MB `.ps2` file with ECC). We want to replace that path with a native service that implements the card file
system directly on the same `.ps2` image file.

Read:
- ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h and MemoryCardFolder.cpp (it implements the PS2 card filesystem)
- ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp, MemoryCardProtocol.cpp
- docs/game-internals.md, docs/iop-changes.md, docs/elf-and-recompilation.md (what is known about libmc2 / MC2_D here)

Answer, in this format:
1. Card image layout: page size, spare/ECC bytes per page, pages per cluster, erase block size, total size of the 8 MB
   `.ps2` file and whether ECC is stored in it (with file:line references).
2. Superblock: offset and every field (name, offset, size, meaning), incl. ifc list, FAT/indirect FAT, alloc offset,
   root directory cluster, cluster counts.
3. FAT: entry format (bit 31 used flag, end-of-chain value), how a file's cluster chain is followed, how free clusters are
   found.
4. Directory entry (512 bytes): every field (mode bits for file/dir/exists, length, created/modified timestamps format,
   first cluster, name, attributes), how "." and ".." work, how a directory's length counts entries.
5. ECC: algorithm used to compute the 16 spare bytes per 512-byte page (point to the function), and whether a reader may
   ignore ECC.
6. Anything in the docs above about the libmc2 / MC2_D RPC interface of THIS game (SIDs, function numbers); if nothing,
   say so (no guessing).
Keep it under 150 lines.
