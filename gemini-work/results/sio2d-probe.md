# SIO2D.IRX / DS2U_D.IRX / MC2_D.IRX Device Detection and SIO2 Elimination

## 1. SIO2D Probe Loop, Device Identification, and Driver Registration
- **Probe Thread (`0x0ae4`)**:
  - Created at module start (`0x0e7c` -> `0x1020`, `thbase#4`, prio 46, stack 0x600).
  - Loops every vblank (`0x0b08`: `jal vblank#5`). Increments global counter `0x1bd8` (`0x0dd4`-`0x0ddc`).
  - Cadence (`0x0b20`-`0x0bc8`, `counter % 60`):
    - Tick 0: Port 0 (Slot 0) -> Pad (Type 1)
    - Tick 15: Port 2 (Slot 0) -> Memory Card (Type 2)
    - Tick 30: Port 1 (Slot 0) -> Pad (Type 1)
    - Tick 45: Port 3 (Slot 0) -> Memory Card (Type 2)
  - Probes only if descriptor is disconnected (`dev+0x10 == 0`). Descriptor base is `0x1c38` (`0x0be0`-`0x0c18`, `dev = 0x1c38 + port*96 + slot*24`, 24 bytes/desc).
- **Per-Port Probes and Device Type Decision**:
  - **Hardcoded Port Types**: SIO2D strictly maps ports 0/1 to Type 1 (Pads) and ports 2/3 to Type 2 (Memory Cards).
  - **Pad Probe (`0x0978`)**: Sends 5-byte command `01 42 00 00 00` (`0x0998`, from `0x1b60`) via `0x1350` / `0x14d8` (`sio2man#51`). Checks RECV1 bits 13 and 16 at `0x15ac`-`0x15ec` (`0x1518`). If connected, returns 5; caller saves `rx_buf[1]` into `dev+8` as mode ID (0x41 digital, 0x73 DualShock analog, 0x79 DualShock 2 analog/pressure).
  - **Memory Card Probe (`0x0a1c`)**: Sends 2-byte command `81 F3` (`0x0a34`-`0x0a40`) via `0x161c` / `0x1880` (`sio2man#51`). `0x17f8` (`0x1838`-`0x1844`) checks if `rx_buf[3] == 0x2B` (PS2 MC device ID). Returns 1 on success; caller sets `dev+8 = 2` (MC mode ID).
- **Driver Table & Invocation (`sio2d:6` register at `0x03ac`, table `0x1bf4[16]`)**:
  - Driver layout: `+0x8` name, `+0xC` probe fn, `+0x10` connect fn, `+0x14` disconnect fn (NULL for all), `+0x28` type (1=Pad, 2=MC).
  - `0x0198` (`sio2d_attach(dev, type)`): Filters `driver->type == type`.
    - Probe: `int (*probe)(DeviceDesc *dev)` at `0x0298`. Argument: `$a0 = dev`. Returns 1 if claimed.
    - Connect: `int (*connect)(DeviceDesc *dev)` at `0x0308`. Argument: `$a0 = dev`. Returns 1 on success.
    - If connect returns 1, `0x0198` stores `dev+0x14 = driver` (`0x0364`); caller sets `dev+0x10 = 1`.
  - Disconnect: Drivers have no `+0x14` callback. Drivers call `sio2d:10(dev)` (`0x014c`) directly to clear `dev+0x10` and `dev+0x14`.
  - Helper `sio2d:9(dev, field)` (`0x0070`): `0 -> dev+8` (mode), `1 -> dev+0` (port), `2 -> dev+4` (slot).

## 2. How MC2_D Gets Connected
- MC2_D registers driver at `0x011c` / `0x01940` (`sio2d:6(0x34c0)`: probe `0x1a50`, connect `0x1af4`, type 2).
- SIO2D probes Port 2 via `0x0a1c(2, 0)` -> returns 1. SIO2D sets `dev+8 = 2` and calls `0x0198(dev, 2)`.
- `0x0198` calls MC2_D probe callback `0x1a50(dev)`:
  - Queries `sio2d:9(dev, 1)` -> port (2). Checks `2 <= port < 4` (`0x01a88`-`0x01aac`).
  - Checks `0x4360[port-2] == 0` (not connected) and `sio2d:9(dev, 0) > 0` (mode). Returns 1.
- `0x0198` calls MC2_D connect callback `0x1af4(dev)`:
  - Stores `dev`, `mode` (2), `port` (2), `slot` (0) in `0x4350`.
  - Calls `0x0320(port - 2)` -> `0x0320(0)`.
  - Inside `0x0320`:
    - Calls `0x08d8(2, 0)`: SIO2 `81 F3` probe check (`0x0364`). If 0, aborts connection (`0x036c`).
    - Calls `0x0650(2, 0)`: card detect/init (already native, returns 1 or 2).
    - If `0x0650 == 2`: sets latch `0x3640[0] = 1`.
    - Calls `0x1d20(2)` (`dbcman#6`, connects socket to EE).
    - Returns 1.
  - Back in `0x1af4`: sets `0x4360[0] = 1`, starts presence thread `0x0418` (`thbase#6` at `0x1ba4`), returns 1.
- `0x0198` stores driver in `dev+0x14`, caller marks `dev+0x10 = 1`.

## 3. Best Routines to Replace Natively
Three minimal function overrides eliminate all SIO2MAN traffic and connect MC2_D:

1. **`SIO2D: 0x0978` (`sio2d_probe_pad`)**:
   ```c
   int sio2d_probe_pad(int port, int slot, uint8_t *buf) {
       return 0; // No controller detected
   }
   ```
   - *Callers*: `0x03ac` (driver register) and `0x0ae4` (vblank loop).
   - *Side effects*: Zero writes. DS2U_D never receives probe/connect callbacks. DS2U_D threads are never started.

2. **`SIO2D: 0x0a1c` (`sio2d_probe_mc`)**:
   ```c
   int sio2d_probe_mc(int port, int slot) {
       return (port == 2 && slot == 0) ? 1 : 0;
   }
   ```
   - *Callers*: `0x03ac` (driver register) and `0x0ae4` (vblank loop).
   - *Side effects*: Zero writes needed. For port 2, caller's existing code (`0x0658` or `0x0d88`) fills `dev+8 = 2` and calls `0x0198(dev, 2)`, running MC2_D's connect path. For port 3, caller skips.

3. **`MC2_D: 0x08d8` (`mc2_sio2_probe_f3`)**:
   ```c
   int mc2_probe_f3(int port, int slot) {
       return (port == 2) ? 1 : 0;
   }
   ```
   - *Callers*: `0x0320` (`0x0364`) and `0x06b0` (`0x04cc`).
   - *Side effects*: Bypasses SIO2 `81 F3` packet assembly (`0x092c`) and SIO2MAN transfer (`0x085c`). Allows `0x0320` to proceed directly to native `0x0650` and `0x1d20` without issuing any SIO2MAN calls.

## 4. Periodic SIO2 Users Audit
- **DS2U_D**: Module start (`0x00000`) creates 2 threads (`0x00029`: `thbase#4`, entry `0x01dc`), but does **not** start them. They are started **only** in connect callback `0x1b24` (`0x1ba4`: `thbase#6`). Since `sio2d_probe_pad` returns 0, `0x1b24` is never called and DS2U_D's polling threads remain dormant forever. No other threads or vblank hooks exist.
- **SIO2D**: Thread `0x0ae4` checks `dev+0x10`. Once Port 2 connects, Port 2 is skipped. Ports 0, 1, 3 remain `dev+0x10 == 0`, calling the stubbed `0x0978` and `0x0a1c` once/sec (instant no-ops returning 0, zero SIO2 traffic).
- **MC2_D**: Connect path starts thread `0x0418` (`0x1ba4`). Thread `0x0418` calls presence check `0x164c` (`0x04c4`). Since `0x164c` is already replaced natively (returning 2), thread `0x0418` issues zero SIO2 traffic.
- **Result**: Exactly zero SIO2MAN transfers issued system-wide.
