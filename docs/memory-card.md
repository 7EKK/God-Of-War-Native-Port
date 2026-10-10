# PS2 memory card: format facts for the native memory-card service

Goal (docs/native-port.md, standing order step 1): replace the hardware path EE libmc2 -> DBCMAN RPC -> MC2_D.IRX ->
SIO2MAN -> SIO2 chip model -> raw card image with a native service that implements the card file system directly on
the same image file (`GOW_MC0`, PCSX2 `.ps2` format).

## How the game reaches the card today
- MC2_D.IRX registers **no RPC server** (no `sifcmd` import). Imports: sysmem, sysclib, thbase, thsemap, vblank, sio2man,
  secrman, cdvdman, dbcman, sio2d. It is a driver under DBCMAN, like the pad driver DS2U_D: the EE talks to both pad and
  card through DBCMAN's RPC (`0x80001300` family).
- So pad + card become native together (the EE pad calls are already native: gow_overrides.cpp scePad2*).

## Card image (`.ps2`), verified in ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h / MemoryCardFile.cpp
- Page = 512 data bytes + 16 spare (ECC) bytes = 528 raw bytes; cluster = 2 pages (1024 data bytes); erase block =
  16 pages (8 clusters). 8 MB card: 16384 pages, file size 8,650,752 bytes (0x840000). Raw offset of data byte b of
  page p: p * 528 + b.
- ECC: 12 bytes per page (3 per 128-byte quarter, Hamming; `CalculateECC` in MemoryCardFile.cpp / MemoryCardFolder.cpp),
  rest of the spare area padding. Readers may ignore it; writers must keep it valid for the real hardware path and
  for PCSX2 (compute it like `CalculateECC`).
- Superblock (page 0): magic "Sony PS2 Memory Card Format " (0x00, 28), version (0x1C, 12), page_len u16 (0x28),
  pages_per_cluster u16 (0x2A), pages_per_block u16 (0x2C), clusters_per_card u32 (0x30), alloc_offset u32 (0x34),
  alloc_end u32 (0x38), rootdir_cluster u32 (0x3C, relative to alloc_offset), backup_block1/2 u32 (0x40/0x44),
  ifc_list[32] u32 (0x50), bad_block_list[32] u32 (0xD0), card_type u8 (0x150, 2 = PS2), card_flags u8 (0x151).
- FAT: superblock ifc_list -> indirect FAT clusters (256 u32 each) -> FAT clusters (256 u32 entries, one per allocatable
  cluster). Entry: bit 31 = in use, bits 0-30 = next cluster (relative to alloc_offset); 0x7FFFFFFF = end of chain
  (stored 0xFFFFFFFF). Free cluster: bit 31 clear. Data cluster c is physical cluster alloc_offset + c.
- Directory entry, 512 bytes (2 per cluster): mode u32 (0x00; 0x10 file, 0x20 dir, 0x8000 exists, 0x0400 PS2 save,
  rwx bits 0x1/0x2/0x4; defaults dir 0x8427, file 0x8497), length u32 (0x04; bytes for a file, entry count incl. "." and
  ".." for a directory), created time (0x08, 8 bytes: unused, sec, min, hour, day, month, year u16), cluster u32 (0x10;
  first cluster, 0xFFFFFFFF for an empty file), dirEntry u32 (0x14; in a subdir's "." = its index in the parent),
  modified time (0x18), attr u32 (0x20), name[32] (0x40). Entry 0 = ".", entry 1 = "..".

## Still to find (next)
- DBCMAN RPC protocol for the card and pad as this game uses it (function numbers, argument/result layout), from
  DBCMAN.IRX / MC2_D.IRX dispatch and the EE libmc2 / libdbc client code.
