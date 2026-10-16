# Task: device detection in SIO2D.IRX / DS2U_D.IRX (read-only analysis of disassembly)

Use read_file only (no shell commands). Give instruction addresses for every claim; mark guesses UNVERIFIED.

Read: ../docs/memory-card.md, ../docs/iop-changes.md (section "SIO2 + DualShock 2"), results/mc2-lowlevel.md (MC2_D
routines; we already replaced MC2_D's card I/O routines 0x0c8c/0x0fdc/0x0a98/0x13a8/0x0650/0x164c with native code),
and the disassemblies requests/dis/SIO2D.txt, requests/dis/DS2U_D.txt, requests/dis/SIO2MAN.txt, requests/dis/MC2_D.txt
(`; -> lib#N` = imported call).

Background: the game's EE reads the controller natively (its scePad2* functions are replaced), and never opens a pad
socket on the IOP. The IOP still runs SIO2D (probes the 4 SIO2 ports for devices and calls the registered drivers) and
DS2U_D (DualShock driver, polls the pad over SIO2 every vblank). We want NO SIO2 traffic at all, while MC2_D still sees
its memory card on port 2 (slot 1).

Find:
1. In SIO2D: the probe loop / thread (which ports, how often), the routine that probes one port over SIO2 and how it
   decides the device type (pad 0x41/0x73/0x79 vs memory card), and how it calls the drivers registered with sio2d:6
   (probe +0xC, connect +0x10, disconnect?) — with argument layouts.
2. How MC2_D gets connected (which sio2d callback leads to MC2_D 0x0320 / 0x0650), so that a native replacement of the
   SIO2D per-port probe can report "memory card" on port 2 and "nothing" on ports 0, 1, 3.
3. The single best routine(s) to replace natively so that: ports 0/1 report no device (DS2U_D never connects or polls),
   port 2 reports a memory card exactly like a real probe would (so MC2_D's connect path runs), port 3 none, and no
   sio2man transfer is ever issued. Give C-like signatures and the exact return values/side effects (globals written,
   device descriptors filled) the replacement must produce.
4. Any other periodic SIO2 users left (e.g. DS2U_D threads already started at module start).
Output under 140 lines.
