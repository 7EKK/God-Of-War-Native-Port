# Memory Card Protocol Analysis

### 1. DBCMAN RPC & Dispatch
* **Registration**: [DBCMAN](file:///C:/Users/lovas/Claude/God%20Of%20War/gemini-work/requests/dis/DBCMAN.txt#L1) registers RPC server `0x8000131C` at `0x1e84` with handler `0x1bcc`.
* **Buffer Passing**: Handler `0x1bcc` receives the `0x2090`-byte buffer in `$a1`. It calls `0x7d0` with `$a0` = `buf[0]` (port), `$a1` = `buf[1]` (cmd), `$a2` = `buf+8`, and delay slot (`01bf8`) `$a3` = `buf+12`.
* **Driver Lookup**: `0x7d0` looks up the driver for the port and calls `0x18cc`. `0x18cc` reads the driver's function table (registered by MC2_D via DBCMAN#4 `0x337c`).
* **Delay Slot Behavior**: In DBCMAN `0x18cc`, the delay slot of `jal 0xecc` (`018f8: addiu $a1, $zero, 5`) overwrites the command index with `5`. Thus, it always calls index 5 of the MC2_D table (`0x43e4`), which points to MC2_D `0x1ef4`.
* **MC2_D Entry**: `0x1ef4` restores `cmd` into `$a1` and invokes the main dispatch `0x1f74` with `$a0`=port, `$a1`=cmd, `$a2`=buf+8, `$a3`=buf+12.
* **Reply**: SIFRPC returns the original `0x2090` buffer, but actual status is sent back asynchronously to EE via DBCMAN's `dbcSignalData` (export 8/9). The reply is an `0x80`-byte structure starting at IOP address `0x4418`.

---

### 2. MC2_D Command Dispatch & Buffer Layout
The switch statement is in `0x1f74` (switching on `cmd`). `0x4410` stores `$a3` (`buf+12`), used as the base pointer for arguments.

* **Argument Layout** (starts at `buf+12`):
  * `buf+12` (Word 3): `page`
  * `buf+16` (Word 4): `count`
  * `buf+20` (Word 5): `page_size` (e.g. 0x200)
  * `buf+24` (Word 6): `spare_size` (e.g. 0x10)
  * `buf+52` (Word 13): **Page data starts here** (`0x4410 + 0x28` offset).

* **Reply Layout** (`0x80` bytes from `0x4418`):
  * `+0x00`: Reserved / internal
  * `+0x04` (`0x441C`): `port`
  * `+0x08` (`0x4420`): **Status word** (cmd return code)
  * `+0x0C` (`0x4424`): Extra reply data (e.g., info for Cmd 3)
  * `+0x10` (`0x4428`): Extra reply data

| Cmd | Address | Function | Notes / Arguments |
|---|---|---|---|
| **1** | `0x2190` | Read Pages | Reads `count` pages from `page`. Data is read into IOP buffer `0x4498`, then streamed to EE via `dbcSignalData`. |
| **2** | `0x22e8` | Write Pages | Writes `count` pages to `page`. Page data is read starting at `buf+52` (`buf+12 + 0x28`), copied to `0x4498`, then written to card. |
| **3** | `0x2904` | Probe / Get Info | Returns 8 bytes of card info (type, free space) in the reply buffer at offsets `+0x0C` (`0x4424`) and `+0x10` (`0x4428`). |
| **4** | `0x2950` | Status Poll | Checks card state. Returns status code directly at `0x4420`. |
| **5** | `0x29e4` | Erase Block | Erases block. Calls `0x2d30`. |
| **6** | `0x29cc` | Unformat / Auth | Card operation calling `0x2f34`. |
| **7** | `0x2a0c` | Unknown | Reads extra args at `buf+40` and `buf+44`. Calls `0x2f98`. |
| **8** | `0x2190` | Read Variant | Same handler as Cmd 1, sets flag at `0x4414`. |
| **12** (0xC) | `0x2a98` | Control / Special | Calls `0x650`. |
| **13** (0xD) | `0x29a8` | Control / Special | Calls `0x568`. |

---

### 3. Status & Return Codes
* **0**: Success (`02680: sw $zero, 0x4420($at)` in write loop)
* **1**: Error / Failure (`02748: addiu $v0, $zero, 1; sw $v0, 0x4420($at)`)
* **2**: Card present, ready, and unchanged

---

### 4. Cmd 4 (Status Poll) Return Value
When a card is present and unchanged, Cmd 4 inspects the port state (`0x544`) and returns **2** in the status word (`0x4420`) (`0298c: addiu $v0, $zero, 2; 0299c: sw $v1, 0x4420($at)`).
