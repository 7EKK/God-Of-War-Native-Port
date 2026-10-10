#pragma once

// GOW-Port: IRX modules compiled ahead of time to native code (tools/iop/iopn_gen.py, docs/native-port.md).
// Each generated module is the image of one IRX as loaded (relocated) at a fixed base; it is used only when the module
// is loaded at that base with the same code bytes (FNV-1a hash of the text section).

#include <cstddef>
#include <cstdint>

namespace ps2x::iop::detail
{
    struct IopCpuState;
    class IopMemory;

    struct IopNativeModule
    {
        const char *name;
        uint32_t base;     // physical load address
        uint32_t textSize; // bytes of code covered
        uint64_t hash;     // iopNativeHash of those bytes
        // Runs compiled code from cpu.pc (inside the module, not in a branch delay slot) and returns the number of
        // instructions executed. Returns when the code reaches an import stub (cpu.pc = the stub, not executed), jumps
        // out of the module, raises an exception or has executed at least `budget` instructions.
        uint32_t (*run)(IopCpuState &cpu, IopMemory &memory, uint32_t budget);
    };

    // generated registry (src/native/iopn_registry.cpp)
    const IopNativeModule *iopNativeModules(size_t &count);

    inline uint64_t iopNativeHash(const uint8_t *data, size_t size)
    {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < size; ++i)
            h = (h ^ data[i]) * 1099511628211ull;
        return h;
    }
}
