// Runtime overrides for God of War (PAL, SCES-53133) on the KIexster god-of-war-recomp PS2Recomp patch stack
// (ref/other-recomp). Their gow_overrides.cpp targets SCUS-97399 addresses; this file carries the parts the PAL
// build needs, with PAL addresses.
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    // Layer 1 of the PAL DVD starts at this sector (tools/iso_extract.py --layout, disc/.disc_layout).
    constexpr uint32_t kLayer1StartLbn = 1902112u;

    void writeGuest32(uint8_t *rdram, uint32_t address, uint32_t value)
    {
        std::memcpy(rdram + (address & 0x01FFFFFCu), &value, sizeof(value));
    }

    // int sceCdReadDvdDualInfo(int *on_dual, unsigned int *layer1_start) (same as their SCUS override).
    void cdReadDvdDualInfo(uint8_t *rdram, R5900Context *ctx, PS2Runtime *)
    {
        writeGuest32(rdram, GPR_U32(ctx, 4), 1u);
        writeGuest32(rdram, GPR_U32(ctx, 5), kLayer1StartLbn);
        SET_GPR_U32(ctx, 2, 1u);
        ctx->pc = GPR_U32(ctx, 31);
    }

    // sceSifLoadStartModuleBuffer for the embedded "ck01" IRX: report a retail-like NO_RESIDENT_END load
    // (their runtime cannot run modules loaded from IOP memory).
    void loadStartModuleBuffer(uint8_t *rdram, R5900Context *ctx, PS2Runtime *)
    {
        writeGuest32(rdram, GPR_U32(ctx, 7), 1u);
        SET_GPR_U32(ctx, 2, 0x40001000u);
        ctx->pc = GPR_U32(ctx, 31);
    }

    void applyGodOfWarPal(PS2Runtime &runtime)
    {
        // Runtime allocations below the ELF so they never overlap the game's own heap (end of .bss upward).
        runtime.setPrivateGuestHeap(0x000A0000u, 0x000FF000u);
        const bool a = runtime.replaceFunction(0x0027B710u, cdReadDvdDualInfo);
        const bool b = runtime.replaceFunction(0x00299928u, loadStartModuleBuffer);
        std::fprintf(stderr, "[gow-pal] sceCdReadDvdDualInfo=%d sceSifLoadStartModuleBuffer=%d\n", a, b);
    }
}

PS2_REGISTER_GAME_OVERRIDE("God of War (PAL)", "SCES_531.33", 0x00100008u, 0xAE7DC7F4u, applyGodOfWarPal);
