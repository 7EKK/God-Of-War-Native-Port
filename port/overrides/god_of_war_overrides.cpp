// Runtime overrides for God of War (PAL, SCES-53133, SCES_531.33).
#include "game_overrides.h"
#include "ps2_runtime.h"

namespace
{
    void applyGodOfWar(PS2Runtime &runtime)
    {
        // The game's own allocator (heap created at 0x138CB0) owns all RAM from the end of .bss up to
        // 0x1FF7FF8, so the runtime's guest data must not live at the default top of RAM or behind
        // SetupHeap. Kernel memory below 0x100000 is unused by the game: the only things it writes
        // there are its kernel patch blobs at 0x75xxx-0x7Fxxx.
        PS2Runtime::GuestReservedLayout layout;
        layout.rpcPacketPoolBase = 0x00080000u;
        layout.rpcServerPoolBase = 0x00090000u;
        layout.tlsPoolBase = 0x000A0000u;
        layout.callbackStackFloor = 0x000B0000u; // 8 x 16 KB async callback stacks
        layout.callbackStackTop = 0x000D0000u;
        layout.heapBase = 0x000D0000u; // 192 KB runtime allocator
        layout.heapLimit = 0x00100000u;
        runtime.setGuestReservedLayout(layout);

        // SDK functions the stripped-ELF analyzer could not bind by signature. Unbound, they talk to IOP
        // RPC services (cdvdfsv, loadfile) that the HLE IOP does not provide and hang or fail.
        ps2_game_overrides::bindAddressHandler(runtime, 0x0027B710u, "sceCdReadDvdDualInfo");
        // Used by sceChk01 (0x2841E0) to run the embedded "ck01" IRX; the game halts at 0x186080 unless it
        // loads and reports success.
        ps2_game_overrides::bindAddressHandler(runtime, 0x00299928u, "sceSifLoadStartModuleBuffer");
    }
}

PS2_REGISTER_GAME_OVERRIDE("God of War (PAL)", "SCES_531.33", 0x00100008u, 0xAE7DC7F4u, applyGodOfWar);
