// GOW-Port: Windows gamepad rumble through XInput (raylib's GLFW backend has none). The DLL is loaded at run time,
// so the runner needs no XInput import library; without it (or without an XInput pad) rumble is simply off.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <cstdint>
#include <utility>

bool ps2PadXInputRumble(uint16_t lowFrequency, uint16_t highFrequency)
{
    using GetStateFn = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
    using SetStateFn = DWORD(WINAPI *)(DWORD, XINPUT_VIBRATION *);
    static const auto functions = [] {
        HMODULE module = LoadLibraryA("xinput1_4.dll");
        if (!module)
            module = LoadLibraryA("xinput9_1_0.dll");
        return std::pair<GetStateFn, SetStateFn>(
            module ? reinterpret_cast<GetStateFn>(GetProcAddress(module, "XInputGetState")) : nullptr,
            module ? reinterpret_cast<SetStateFn>(GetProcAddress(module, "XInputSetState")) : nullptr);
    }();
    if (!functions.first || !functions.second)
        return false;
    // raylib's gamepad index does not map to an XInput user index; every connected XInput pad gets the state.
    XINPUT_VIBRATION vibration{lowFrequency, highFrequency};
    bool any = false;
    for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user)
    {
        XINPUT_STATE state{};
        if (functions.first(user, &state) == ERROR_SUCCESS)
            any = functions.second(user, &vibration) == ERROR_SUCCESS || any;
    }
    return any;
}
#endif
