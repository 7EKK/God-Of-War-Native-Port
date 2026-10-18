// "God of War.exe": the launcher of the shipped PC folder (tools/pc_release/make_pc_folder.ps1, docs/build-and-run.md):
//   God of War.exe   this launcher: settings window (resolution, fullscreen), then starts the game
//   bin\             GodOfWar.exe (the runner, ps2EntryRunner) and its DLLs
//   data\            the extracted USA disc (SCUS_973.99, PART1/2.PAK, ...)
//   saves\           GodOfWar.sav, the save file (a PS2 memory-card image) + GodOfWar.sav.bak, the copy from before
//                    the last start
//   settings.ini     [Video] Width, Height, Fullscreen; other sections may add their own keys
// It sets what play.cmd sets (release settings, no diagnostics) plus the video settings and runs the game with bin\ as
// working directory. "God of War.exe --play" skips the window and starts with the saved settings.
#include <windows.h>

#include <algorithm>
#include <functional>
#include <cwchar>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
constexpr int kIdResolution = 101, kIdFullscreen = 102, kIdPlay = IDOK, kIdQuit = IDCANCEL;

struct Settings
{
    int width = 1920, height = 1080;
    bool fullscreen = false;
};

std::wstring g_root;
Settings g_settings;
std::vector<std::pair<int, int>> g_modes;
HWND g_resolution = nullptr, g_fullscreen = nullptr;
bool g_play = false;

std::wstring folderOf(const std::wstring &path)
{
    return path.substr(0, path.find_last_of(L"\\/"));
}

bool exists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring iniPath()
{
    return g_root + L"\\settings.ini";
}

Settings loadSettings()
{
    Settings s;
    const std::wstring ini = iniPath();
    s.width = static_cast<int>(GetPrivateProfileIntW(L"Video", L"Width", s.width, ini.c_str()));
    s.height = static_cast<int>(GetPrivateProfileIntW(L"Video", L"Height", s.height, ini.c_str()));
    s.fullscreen = GetPrivateProfileIntW(L"Video", L"Fullscreen", 0, ini.c_str()) != 0;
    if (s.width < 320 || s.height < 240)
        s = Settings{};
    return s;
}

void saveSettings(const Settings &s)
{
    const std::wstring ini = iniPath();
    WritePrivateProfileStringW(L"Video", L"Width", std::to_wstring(s.width).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Video", L"Height", std::to_wstring(s.height).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Video", L"Fullscreen", s.fullscreen ? L"1" : L"0", ini.c_str());
}

// the primary monitor's modes (largest first), at least 1024x576, plus the saved one
std::vector<std::pair<int, int>> displayModes()
{
    std::vector<std::pair<int, int>> modes;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &mode); ++i)
        if (mode.dmPelsWidth >= 1024 && mode.dmPelsHeight >= 576)
            modes.emplace_back(static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight));
    modes.emplace_back(g_settings.width, g_settings.height);
    std::sort(modes.begin(), modes.end(), std::greater<>());
    modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
    return modes;
}

HWND control(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND hwnd = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return hwnd;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        control(hwnd, L"STATIC", L"Resolution", 0, 20, 24, 90, 20, 0);
        g_resolution = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 120, 20, 200, 300,
                               kIdResolution);
        int selected = 0;
        for (size_t i = 0; i < g_modes.size(); ++i)
        {
            const std::wstring text = std::to_wstring(g_modes[i].first) + L" x " + std::to_wstring(g_modes[i].second);
            SendMessageW(g_resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
            if (g_modes[i] == std::make_pair(g_settings.width, g_settings.height))
                selected = static_cast<int>(i);
        }
        SendMessageW(g_resolution, CB_SETCURSEL, selected, 0);
        g_fullscreen = control(hwnd, L"BUTTON", L"Fullscreen (F11 switches in game)", BS_AUTOCHECKBOX | WS_TABSTOP, 120,
                               56, 220, 20, kIdFullscreen);
        SendMessageW(g_fullscreen, BM_SETCHECK, g_settings.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
        control(hwnd, L"BUTTON", L"Play", BS_DEFPUSHBUTTON | WS_TABSTOP, 120, 100, 95, 30, kIdPlay);
        control(hwnd, L"BUTTON", L"Quit", WS_TABSTOP, 225, 100, 95, 30, kIdQuit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdPlay)
        {
            const LRESULT index = SendMessageW(g_resolution, CB_GETCURSEL, 0, 0);
            if (index >= 0 && static_cast<size_t>(index) < g_modes.size())
                std::tie(g_settings.width, g_settings.height) = g_modes[static_cast<size_t>(index)];
            g_settings.fullscreen = SendMessageW(g_fullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_play = true;
            DestroyWindow(hwnd);
        }
        else if (LOWORD(wParam) == kIdQuit)
            DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// the settings window; true when the player pressed Play
bool askSettings(HINSTANCE instance)
{
    g_modes = displayModes();
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"GodOfWarLauncher";
    RegisterClassW(&wc);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 360, 150};
    AdjustWindowRect(&rect, style, FALSE);
    const int w = rect.right - rect.left, h = rect.bottom - rect.top;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"God of War", style, (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                                (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h, nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (IsDialogMessageW(hwnd, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return g_play;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int)
{
    wchar_t self[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, self, static_cast<DWORD>(sizeof(self) / sizeof(self[0])));
    g_root = folderOf(self);
    const std::wstring bin = g_root + L"\\bin";
    const std::wstring runner = bin + L"\\GodOfWar.exe";
    const std::wstring elf = g_root + L"\\data\\SCUS_973.99";
    if (!exists(runner) || !exists(elf))
    {
        MessageBoxW(nullptr, (L"Missing game files next to the launcher:\n" + runner + L"\n" + elf).c_str(), L"God of War",
                    MB_ICONERROR);
        return 1;
    }

    g_settings = loadSettings();
    const bool direct = commandLine != nullptr && std::wcsstr(commandLine, L"--play") != nullptr;
    if (!direct && !askSettings(instance))
        return 0;
    saveSettings(g_settings);

    // saves: one file in saves\, copied to .bak before every start so a broken save never costs the previous one
    const std::wstring saves = g_root + L"\\saves";
    CreateDirectoryW(saves.c_str(), nullptr);
    const std::wstring save = saves + L"\\GodOfWar.sav";
    if (exists(save))
        CopyFileW(save.c_str(), (save + L".bak").c_str(), FALSE);

    SetEnvironmentVariableW(L"PS2X_GS_WORKER", L"1");
    SetEnvironmentVariableW(L"GOW_GS_FINISH_ASINCRONO", L"1");
    if (GetEnvironmentVariableW(L"GOW_AUDIO", nullptr, 0) == 0) // test runs mute with GOW_AUDIO=0
        SetEnvironmentVariableW(L"GOW_AUDIO_DEVICE", L"default");
    SetEnvironmentVariableW(L"GOW_MC0", save.c_str());
    SetEnvironmentVariableW(L"GOW_WINDOW",
                            (std::to_wstring(g_settings.width) + L"x" + std::to_wstring(g_settings.height)).c_str());
    SetEnvironmentVariableW(L"GOW_FULLSCREEN", g_settings.fullscreen ? L"1" : L"0");

    std::wstring command = L"\"" + runner + L"\" \"" + elf + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(runner.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, bin.c_str(), &startup, &process))
    {
        MessageBoxW(nullptr, L"Could not start bin\\GodOfWar.exe.", L"God of War", MB_ICONERROR);
        return 1;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
