### 1. Card Image Layout
- **Page data size**: 512 bytes (`0x200`) ([`MemoryCardFolder.h:137, 231`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L137-L231)).
- **Spare / ECC bytes per page**: 16 bytes (`0x10`) ([`MemoryCardFolder.h:234`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L234)). Raw page size = 528 bytes (`0x210`) ([`MemoryCardFolder.h:235`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L235), [`MemoryCardFile.cpp:31`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L31)).
- **Pages per cluster**: 2 pages (cluster data = 1024 bytes, raw cluster = 1056 bytes) ([`MemoryCardFolder.h:24, 232, 236`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L24-L236)).
- **Erase block size**: 16 pages = 8 clusters (block data = 8192 bytes, raw block = 8448 bytes) ([`MemoryCardFolder.h:25, 233, 237`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L25-L237), [`MemoryCardFile.cpp:33`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L33)).
- **Total size of 8 MB `.ps2` file**: 8,650,752 bytes (`0x840000`, 8448 KiB = ~8.25 MiB) calculated as `TotalPages (16384) * PageSizeRaw (528)` or `8 * MC2_MBSIZE (1,081,344)` ([`MemoryCardFolder.h:238-241`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L238-L241), [`MemoryCardFile.cpp:31, 844, 1020`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L31-L1020)).
- **ECC storage**: **YES**, ECC/spare bytes are stored in the `.ps2` file. Every 512-byte page is immediately followed by its 16 spare bytes ([`MemoryCardFile.cpp:84-121, 297-313, 407-413`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L84-L413), [`MemoryCardProtocol.cpp:131`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardProtocol.cpp#L131)).

### 2. Superblock
- **Offset**: Block 0, sector 0, byte offset `0x00000000` ([`MemoryCardFolder.h:18-38`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L18-L38), [`MemoryCardFolder.cpp:745-748`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L745-L748)).
- **Fields** ([`MemoryCardFolder.h:19-38`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L19-L38)):
  - `magic[28]` (offset `0x00`, 28 bytes): Magic string `"Sony PS2 Memory Card Format "`
  - `version[12]` (offset `0x1C`, 12 bytes): Filesystem version string (e.g. `"1.2.0.0"`)
  - `page_len` (offset `0x28`, `u16`, 2 bytes): Page size in bytes without spare (512)
  - `pages_per_cluster` (offset `0x2A`, `u16`, 2 bytes): Pages per cluster (2)
  - `pages_per_block` (offset `0x2C`, `u16`, 2 bytes): Pages per erase block (16)
  - `unused` (offset `0x2E`, `u16`, 2 bytes): Alignment padding (0)
  - `clusters_per_card` (offset `0x30`, `u32`, 4 bytes): Total clusters on card (8192 for 8 MB)
  - `alloc_offset` (offset `0x34`, `u32`, 4 bytes): First allocatable data cluster index (after system blocks/IFC/FAT)
  - `alloc_end` (offset `0x38`, `u32`, 4 bytes): Number of allocatable data clusters relative to `alloc_offset`
  - `rootdir_cluster` (offset `0x3C`, `u32`, 4 bytes): Root directory's first cluster relative to `alloc_offset` (typically 0)
  - `backup_block1` (offset `0x40`, `u32`, 4 bytes): Block index of primary backup block (e.g. 1023)
  - `backup_block2` (offset `0x44`, `u32`, 4 bytes): Block index of secondary backup block (e.g. 1022)
  - `padding0x48` (offset `0x48`, `u64`, 8 bytes): Reserved padding
  - `ifc_list[32]` (offset `0x50`, 128 bytes, 32 x `u32`): Physical cluster numbers holding Indirect FAT Clusters (IFC)
  - `bad_block_list[32]` (offset `0xD0`, 128 bytes, 32 x `u32`): Physical erase block numbers marked bad/defective
  - `card_type` (offset `0x150`, `u8`, 1 byte): Memory card type (2 = PS2 card)
  - `card_flags` (offset `0x151`, `u8`, 1 byte): Status flags (bit 0 = CF_FORMATTED)

### 3. FAT (File Allocation Table)
- **Hierarchy**: Superblock `ifc_list` points to Indirect FAT clusters; each IFC contains up to 256 `u32` pointers to FAT clusters; each FAT cluster contains 256 `u32` entries corresponding to allocatable data clusters ([`MemoryCardFolder.h:230, 256-265`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L230-L265), [`MemoryCardFolder.cpp:277-296`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L277-L296)).
- **Entry format** ([`MemoryCardFolder.h:244-246`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L244-L246)):
  - **Bit 31** (`0x80000000`, `DataClusterInUseMask`): In-use flag. Set = cluster allocated/used; Clear = free.
  - **Bits 0–30** (`0x7FFFFFFF`, `NextDataClusterMask`): Next cluster in the chain relative to `alloc_offset`.
  - **End-of-chain value**: `0x7FFFFFFF` (`LastDataCluster`). Combined with in-use bit 31, end-of-chain entry value is `0xFFFFFFFF` (`LastDataCluster | DataClusterInUseMask`).
- **Following a cluster chain**: Start at entry's first `cluster`; next cluster = `fat[current] & 0x7FFFFFFF`; continue until next is `0x7FFFFFFF` ([`MemoryCardFolder.cpp:386-396, 680-700`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L386-L700)). Physical raw byte offset on `.ps2` = `(alloc_offset + cluster) * 1056`.
- **Finding free clusters**: Scan FAT sequentially from index 0 to `alloc_end`; the first cluster with `(fat[i] & 0x80000000) == 0` is free ([`MemoryCardFolder.cpp:351-366`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L351-L366)).

### 4. Directory Entry (512 bytes)
Stored in directory clusters (2 entries per 1024-byte cluster, [`MemoryCardFolder.h:128-132`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L128-L132)).
- **Fields** ([`MemoryCardFolder.h:96-108`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L96-L108)):
  - `u32 mode` (offset `0x00`, 4 bytes): Permissions & type flags ([`MemoryCardFolder.h:74-92`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L74-L92)):
    - `0x0001` Read, `0x0002` Write, `0x0004` Execute, `0x0008` CopyProtected
    - `0x0010` File, `0x0020` Directory, `0x0400` PS2 Save, `0x2000` Hidden
    - `0x8000` Mode_Used (entry active/exists; 0 = unused/deleted; 0xFFFFFFFF = unformatted)
    - Defaults: Directory = `0x8427`, File = `0x8497`.
  - `u32 length` (offset `0x04`, 4 bytes): File size in bytes for files; total entry count for directories (including `.` and `..`).
  - `MemoryCardFileEntryDateTime timeCreated` (offset `0x08`, 8 bytes): `unused` (`u8`), `second` (`u8`), `minute` (`u8`), `hour` (`u8`), `day` (`u8`), `month` (`u8`, 1–12), `year` (`u16` LE) ([`MemoryCardFolder.h:41-51`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L41-L51)).
  - `u32 cluster` (offset `0x10`, 4 bytes): First cluster relative to `alloc_offset`. Empty files (0 bytes) use `0xFFFFFFFF` (`EmptyFileCluster`) ([`MemoryCardFolder.h:123`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.h#L123)).
  - `u32 dirEntry` (offset `0x14`, 4 bytes): In `.` entry of a subdir, stores parent directory's entry index for this subdir.
  - `MemoryCardFileEntryDateTime timeModified` (offset `0x18`, 8 bytes): Same 8-byte timestamp structure.
  - `u32 attr` (offset `0x20`, 4 bytes): File attributes (typically 0).
  - `u8 padding[0x1C]` (offset `0x24`, 28 bytes): Reserved padding.
  - `u8 name[0x20]` (offset `0x40`, 32 bytes): Null-terminated filename/dirname (up to 31 chars).
  - `u8 unused[0x1A0]` (offset `0x60`, 416 bytes): Padding to complete 512 bytes.
- **`.` and `..` semantics**:
  - `.` (entry 0): Refers to the directory itself (`cluster` = start cluster; `dirEntry` = entry index in parent).
  - `..` (entry 1): Refers to the parent directory (`cluster` = parent start cluster; in root dir, points to root) ([`MemoryCardFolder.cpp:301-310, 529-539`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L301-L539)).
- **Directory length entry counting**: `length` counts total entries including `.` and `..` (empty directory has `length = 2`). Each added item increments `length` by 1. Since each cluster holds 2 entries, directory clusters required = `ceil(length / 2)` ([`MemoryCardFolder.cpp:403-421, 504, 628`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L403-L628)).

### 5. ECC
- **Algorithm & Function**:
  - `FolderMemoryCard::CalculateECC(u8* ecc, const u8* data)` ([`MemoryCardFolder.cpp:1910-1953`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L1910-L1953)) and `CalculateECC(u8* buf)` ([`MemoryCardFile.cpp:43-82`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L43-L82)).
  - Each 512-byte page is split into four 128-byte (`0x80`) blocks. For each 128-byte block, a 3-byte parity code (column parity byte + 2 line parity bytes, Hamming code) is calculated via table lookups ([`MemoryCardFolder.cpp:983-987`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L983-L987), [`MemoryCardFile.cpp:105-110`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L105-L110)).
  - 4 blocks x 3 bytes = 12 ECC bytes; the remaining 4 bytes of the 16-byte spare area are padding / null bytes (or 0xFF).
- **Ignoring ECC**: **YES**, a native filesystem reader may completely ignore ECC. Filesystem data is fully contained in the 512-byte data pages; the reader simply skips the 16 spare bytes per page (stride 528). PCSX2 does not validate ECC on file reads ([`MemoryCardFile.cpp:427-438`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFile.cpp#L427-L438)) and ignores incoming ECC on writes ([`MemoryCardFolder.cpp:1491-1495`](file:///C:/Users/lovas/Claude/God%20Of%20War/ref/pcsx2-src/pcsx2/SIO/Memcard/MemoryCardFolder.cpp#L1491-L1495)).

### 6. Game-Specific libmc2 / MC2_D RPC Interface
- **Nothing is documented** in [`docs/game-internals.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/game-internals.md), [`docs/iop-changes.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/iop-changes.md), or [`docs/elf-and-recompilation.md`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/elf-and-recompilation.md) regarding SIDs or RPC function numbers for `libmc2` / `MC2_D`.
- The docs only record that EE links SDK 3.0 `libmc2` ([`elf-and-recompilation.md:9`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/elf-and-recompilation.md#L9)), IOP loads `mc2_d` without arguments ([`game-internals.md:14-15`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/game-internals.md#L14-L15)), `MC2_D` imports `sio2man, sio2d, dbcman, secrman[4,5], cdvdman[24]` ([`game-internals.md:88`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/game-internals.md#L88)), and that it communicates through SIO2 to the card ([`game-internals.md:85`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/game-internals.md#L85), [`iop-changes.md:95`](file:///C:/Users/lovas/Claude/God%20Of%20War/docs/iop-changes.md#L95)).
ude/God%20Of%20War/docs/iop-changes.md#L95)).
