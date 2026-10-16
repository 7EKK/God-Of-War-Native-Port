The analysis is complete. The results have been written to `results/mc2-lowlevel.md`.

# Low-Level Card Routines of MC2_D.IRX

Analysis of `MC2_D.IRX` disassembly (`requests/dis/MC2_D.txt`) for native raw card image replacement (page = 512 data + 16 spare = 528 bytes).

### 1. `0x0c8c` (Wrapper) / `0x0d1c` (Core): Read Page
* **Function**: Reads a physical page (512 data + 16 spare bytes) from card via SIO2.
  * `0x0d1c` invokes `0x0f04` (SIO2 subcmd `0x23`: page address setup + XOR checksum `0x186c`), loops `0x0f98` (SIO2 subcmd `0x43`: chunk transfer up to 128 bytes), checks chunk XOR checksums (`0x0e5c`), and finishes with `0x1758` (SIO2 subcmd `0x81`: termination).
  * `0x0c8c` wraps `0x0d1c` with up to 3 retries (`0x0ce4`).
* **Arguments**:
  * `$a0`: Port (2 or 3)
  * `$a1`: Slot (0)
  * `$a2`: Physical page number
  * `$a3`: Page transfer size in bytes (528, from global `0x4cdc`)
  * Stack `0x10($sp)` (in caller's frame, loaded to `$fp` at `0x0d44`): Destination buffer in IOP RAM.
* **Return**: `$v0` = 1 on success, 0 on error.
* **Data Destination**: Stored directly into the buffer passed on stack (`$fp`). In Cmd 1 (`0x21ec`), buffer is `0x4498`. Layout: 512 bytes data followed by 16 bytes spare (ECC).
* **Callers**: Cmd 1 & 8 (`0x21ec`), Cmd 2 read-verify (`0x2648`, `0x2724`), Cmd 5 backup check (`0x2d94`, `0x2e08`), Cmd 7 block copy (`0x3090`, `0x3114`).

### 2. `0x0fdc` (Wrapper) / `0x106c` (Core): Write Page
* **Function**: Writes a physical page (512 data + 16 spare bytes) to card via SIO2.
  * `0x106c` invokes `0x11d4` (SIO2 subcmd `0x22`: write page address setup), loops `0x1268` (SIO2 subcmd `0x42`: write data chunk with XOR checksum), and finishes with `0x1758` (SIO2 subcmd `0x81`: termination).
  * `0x0fdc` wraps `0x106c` with up to 3 retries (`0x1034`).
* **Arguments**:
  * `$a0`: Port (2 or 3)
  * `$a1`: Slot (0)
  * `$a2`: Physical page number
  * `$a3`: Source buffer pointer in IOP RAM (e.g. `0x4498`)
  * Stack `0x10($sp)` (loaded to `$s5` at `0x1094`): Size in bytes (528).
* **Return**: `$v0` = 1 on success, 0 on error.
* **Callers**: Cmd 2 (`0x244c`, `0x2544`, `0x2628`), Cmd 7 block copy (`0x30d4`).

### 3. `0x0a98` (Wrapper) / `0x0b08` (Core): Erase Block
* **Function**: Erases an erase block (16 pages = 8 clusters) on the card.
  * `0x0b08` invokes `0x0bf8` (SIO2 subcmd `0x21`: erase block address setup), executes via `0x1794` (SIO2 subcmd `0x82`: erase commit), and polls ready state via `0x0a00` (SIO2 subcmd `0x12` probe) in a loop with `thbase#33` (`DelayThread(1000)`) at `0x0b90`.
  * `0x0a98` wraps `0x0b08` with up to 3 retries (`0x0ad8`).
* **Arguments**:
  * `$a0`: Port (2 or 3)
  * `$a1`: Slot (0)
  * `$a2`: First page number of the block (e.g. `block_index * 16`).
* **Return**: `$v0` = 1 on success, 0 on error.
* **Callers**: Cmd 2 erase-before-write (`0x255c`), Cmd 5 erase block (`0x2e5c`), Cmd 7 (`0x2a60`, `0x3020`).

### 4. `0x13a8` (Wrapper) / `0x1418` (Core): Read Card Specs (Cmd 3)
* **Function**: Queries card geometry/specs via SIO2 subcmd `0x26` (`0x14e0`). Reads 8 response bytes from SIO2 buffer `0x3d23`.
* **Arguments**:
  * `$a0`: Port (2 or 3)
  * `$a1`: Slot (0)
  * `$a2`: Destination buffer pointer (8 bytes). Passed as `0x4498` in Cmd 3 (`0x2918`).
* **Return**: `$v0` = 1 on success, 0 on error.
* **Data Destination**: 8 bytes written to destination buffer, then copied by Cmd 3 (`0x293c`-`0x294c`) to reply buffer `0x4424` / `0x4428`. Format: Word 0 = `(pages_per_block << 16) | page_size` (`0x00100200`), Word 1 = `(flags << 16) | total_pages` (`0x00004000` for 8MB).

### 5. `0x164c` (Wrapper) / `0x16ac` (Core): Card Presence Probe
* **Function**: Sends SIO2 subcmd `0x28` (`0x1718`) to probe card presence.
* **Arguments**: `$a0` = Port, `$a1` = Slot.
* **Return**: `$v0`: 0 = No card / error, 1 = Card present (changed), 2 = Card present (unchanged).
* **Callers**: Cmd 4 (`0x2958`), `0x06b0` (`0x06fc`), background vblank thread `0x0418` (`0x04c4`).

### 6. `0x0650` (Wrapper) / `0x06b0` (Core): Card Detect & Init (Cmd 12)
* **Function**: Detects and initializes card hardware. Sends SIO2 probe `0xF3` (`0x08d8`), queries SIO2 device `sio2d#12`, calls `0x164c` (subcmd `0x28`), sends subcmd `0x27` (`0x1520`), and subcmd `0xBF` (`0x17d4`).
* **Arguments**: `$a0` = Port, `$a1` = Slot.
* **Return**: `$v0`: 0 = Error / absent, 1 = Ready / unchanged, 2 = Ready / changed.
* **Callers**: Cmd 12 (`0x2aa0`), background card connect `0x0320` (`0x038c`).

### 7. `0x0544` & `0x0568`: Port State Check / Clear Latch (Cmd 4 & 13)
* **What it reads/writes**:
  * Reads/writes global word array `0x3640[port - 2]`. Port 2 is `0x3640`, Port 3 is `0x3644`.
  * `0x0544` returns `(0x3640[port-2] > 0) ? 2 : 0`.
  * `0x0568` sets `0x3640[port-2] = $a2` (called with `$a2 = 0` by Cmd 13 at `0x29bc`).
  * `0x3640` is set to `1` by driver init `0x011c` and when card connect/change is detected (`0x0238`, `0x03b4`).
* **Cmd 4 (`0x2950`) status return values in `0x4420`**:
  * `0`: No card present (returned if `0x164c` returns 0).
  * `1`: Card present, ready, and unchanged (returned if `0x164c != 0` and `0x0544 == 0`).
  * `2`: Card present, changed / newly inserted (returned if `0x164c != 0` and `0x0544 == 2`).
* **Dispatch impact**: In `0x1f74` (`0x1fe4`-`0x2008`), if `0x0544` returns 2 ("card changed"), all commands except Cmd 4 and Cmd 13 are rejected with status 2 until EE issues Cmd 13 (`0x29a8`) to clear the latch.

### 8. Authentication (`secrman`)
* `secrman#4` (`0x332c`) and `secrman#5` (`0x3334`) are called **only** in module start `0x0590` and stop `0x0628`.
* They register/unregister SIO2 callback handlers (`0x1898`, `0x18d0`).
* **No command handler (1, 2, 3, 4, 5, 6, 7, 8, 12, 13) calls secrman.** MC2_D does not perform MagicGate auth on save files.
* `0x0590` can safely be made a no-op returning 1.

### 9. Recommended Cut List
The best cut point is at the 6 retry wrappers (`0x0c8c`, `0x0fdc`, `0x0a98`, `0x13a8`, `0x0650`, `0x164c`). Cutting here eliminates all SIO2 packet assembly, chunking, timeouts, and hardware SIO2MAN calls while keeping all MC2_D dispatch, buffer unpacking, ECC calculation, and EE RPC responses intact.

1. **`0x0c8c`**: `int mc_read_page(int port, int slot, uint32_t page, uint32_t size, void* buf)`
   * Read `size` (528) bytes from image offset `page * 528` into `buf`. Return 1.
2. **`0x0fdc`**: `int mc_write_page(int port, int slot, uint32_t page, const void* buf, uint32_t size)`
   * Write `size` (528) bytes from `buf` to image offset `page * 528`. Return 1.
3. **`0x0a98`**: `int mc_erase_block(int port, int slot, uint32_t page)`
   * Fill 16 * 528 bytes (8448 bytes) at `(page & ~15) * 528` with `0xFF`. Return 1.
4. **`0x13a8`**: `int mc_get_specs(int port, int slot, uint32_t* buf8)`
   * `buf8[0] = 0x00100200; buf8[1] = 0x00004000;` (512 page size, 16 pages/block, 16384 pages). Return 1.
5. **`0x0650`**: `int mc_init_card(int port, int slot)`
   * Return 1 (present and ready).
6. **`0x164c`**: `int mc_probe_card(int port, int slot)`
   * Return 2 (present and unchanged).
7. **`0x0590`**: `int mc_secr_init()`
   * Return 1 (no-op, bypass secrman).
