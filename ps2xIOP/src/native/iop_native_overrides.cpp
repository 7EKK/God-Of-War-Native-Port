// GOW-Port: IRX functions replaced by native C++ (see iop_native.h). Each entry must also be listed in
// tools/iop/native_overrides.txt so the generated code hands control to the dispatcher at that address.
#include "native/iop_native.h"

#include "emulator/core/iop_cpu.h"
#include "emulator/core/iop_memory.h"
#include "emulator/core/sio2.h"

#include <vector>

namespace ps2x::iop::detail
{
    namespace
    {
        // ---- MC2_D.IRX: the memory-card driver's card I/O (docs/memory-card.md) ----------------------------------
        // MC2_D keeps its command dispatch, buffers and replies to the EE (the card file system runs on the EE in
        // libmc2); only the routines that talked to the card through SIO2 transfers are native: they read and write
        // the card image directly. Ports 2/3 are memory-card slots 1/2.

        constexpr uint32_t kA0 = 4, kA1 = 5, kA2 = 6, kA3 = 7, kV0 = 2, kSp = 29;

        MemoryCard *cardFor(IopMemory &memory, uint32_t port)
        {
            Sio2 &sio2 = memory.sio2();
            sio2.prepareCard(port & 1u);
            MemoryCard &card = sio2.card(port & 1u);
            return card.present() ? &card : nullptr;
        }

        // 0x0c8c: int read_page(port, slot, page, size, void *iopDst /* stack +0x10 */); 1 = ok, 0 = error
        void mcReadPage(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            MemoryCard *card = cardFor(memory, cpu.gpr[kA0]);
            const uint32_t page = cpu.gpr[kA2], size = cpu.gpr[kA3];
            const uint32_t destination = memory.load32(cpu.gpr[kSp] + 0x10u);
            if (!card || size == 0u || size > MemoryCard::RawPageSize * MemoryCard::PagesPerBlock)
            {
                cpu.gpr[kV0] = 0u;
                return;
            }
            std::vector<uint8_t> data(size);
            card->setAddress(page);
            card->read(data.data(), size);
            cpu.gpr[kV0] = memory.writeRam(destination, data.data(), size) ? 1u : 0u;
        }

        // 0x0fdc: int write_page(port, slot, page, const void *iopSrc, size /* stack +0x10 */)
        void mcWritePage(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            MemoryCard *card = cardFor(memory, cpu.gpr[kA0]);
            const uint32_t page = cpu.gpr[kA2], source = cpu.gpr[kA3];
            const uint32_t size = memory.load32(cpu.gpr[kSp] + 0x10u);
            std::vector<uint8_t> data(size);
            if (!card || size == 0u || size > MemoryCard::RawPageSize * MemoryCard::PagesPerBlock ||
                !memory.readRam(source, data.data(), size))
            {
                cpu.gpr[kV0] = 0u;
                return;
            }
            card->setAddress(page);
            card->write(data.data(), size); // NAND: bits only go from 1 to 0, the driver erases first
            cpu.gpr[kV0] = 1u;
        }

        // 0x0a98: int erase_block(port, slot, firstPageOfBlock)
        void mcEraseBlock(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            MemoryCard *card = cardFor(memory, cpu.gpr[kA0]);
            if (!card)
            {
                cpu.gpr[kV0] = 0u;
                return;
            }
            card->setAddress(cpu.gpr[kA2] & ~(MemoryCard::PagesPerBlock - 1u));
            card->eraseBlock();
            cpu.gpr[kV0] = 1u;
        }

        // 0x13a8: int get_specs(port, slot, u32 out[2]): page size / pages per block / page count (card command 0x26)
        void mcGetSpecs(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            if (!cardFor(memory, cpu.gpr[kA0]))
            {
                cpu.gpr[kV0] = 0u;
                return;
            }
            const uint32_t specs[2] = {(MemoryCard::PagesPerBlock << 16) | MemoryCard::PageSize, MemoryCard::PageCount};
            cpu.gpr[kV0] = memory.writeRam(cpu.gpr[kA2], specs, sizeof(specs)) ? 1u : 0u;
        }

        // 0x0650: int init_card(port, slot): 0 = no card, 1 = ready (2 only when the card's terminator differs from
        // the 0x55 the driver sets, which a card image never does)
        void mcInitCard(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            cpu.gpr[kV0] = cardFor(memory, cpu.gpr[kA0]) ? 1u : 0u;
        }

        // 0x164c: int probe(port, slot): 0 = no card, 1 = present (terminator 0x55 echoed twice)
        void mcProbe(IopCpuState &cpu, IopMemory &memory, IopHost &)
        {
            cpu.gpr[kV0] = cardFor(memory, cpu.gpr[kA0]) ? 1u : 0u;
        }

        const IopNativeOverride kOverrides[] = {
            {"mc2_d_irx", 0x0c8cu, "mc2_d read_page", &mcReadPage},
            {"mc2_d_irx", 0x0fdcu, "mc2_d write_page", &mcWritePage},
            {"mc2_d_irx", 0x0a98u, "mc2_d erase_block", &mcEraseBlock},
            {"mc2_d_irx", 0x13a8u, "mc2_d get_specs", &mcGetSpecs},
            {"mc2_d_irx", 0x0650u, "mc2_d init_card", &mcInitCard},
            {"mc2_d_irx", 0x164cu, "mc2_d probe", &mcProbe},
        };
    }

    const IopNativeOverride *iopNativeOverrides(size_t &count)
    {
        count = sizeof(kOverrides) / sizeof(kOverrides[0]);
        return kOverrides;
    }
}
