# Task: memory-card command protocol of MC2_D.IRX under DBCMAN (read-only analysis of disassembly)

Use read_file only (no shell commands). Mark every conclusion you could not tie to specific instructions as UNVERIFIED.
Give instruction addresses for every claim.

Read first: ../docs/memory-card.md (what is known: the game's EE libmc2 sends 0x2090-byte buffers to RPC server
0x8000131C; send words: `0, cmd, 0x2080, page, count, 0x200, 0x10, 0x4000`; cmds seen 1 (read pages), 3, 4, 6, 0xC,
0xD; write/erase cmds not seen yet).

Disassembly (R3000, module base 0; `; -> lib#N` marks calls to imported functions):
- requests/dis/DBCMAN.txt (the RPC server side: sifcmd imports; it forwards socket requests to drivers registered
  with it)
- requests/dis/MC2_D.txt (the memory-card driver; registers itself with dbcman, talks to sio2man/sio2d)

Find and report:
1. In DBCMAN: how the 0x8000131C (socket) RPC server is registered, its handler, and how it passes the received
   0x2090-byte buffer to the driver (which function pointer / table entry of MC2_D is called, with which arguments),
   and what is copied back to the EE (reply size and from where).
2. In MC2_D: the command dispatch (switch on the `cmd` word, i.e. word 1 of the buffer). For every command number list
   what it does (read pages, write pages, erase block, get card info/status, probe/auth (secrman), ...), which buffer
   words it reads as arguments, and where it writes results in the reply buffer (offsets of status word(s), page data,
   spare bytes). Note the layout used for page data in the 0x2090 buffer (where the 0x2080 bytes of data start).
3. The status/return codes written into the reply (success, no card, error) with their values.
4. What cmd 4 (status poll) returns when a card is present and unchanged.
Output: a table of commands + the reply layout, under 120 lines.
