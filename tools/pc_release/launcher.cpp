// "God of War.exe": starts the PC port from a self-contained folder (tools/pc_release/make_pc_folder.ps1):
//   God of War.exe   this launcher
//   game\            ps2EntryRunner.exe and its DLLs
//   disc\            the extracted USA disc (SCUS_973.99, PART1/2.PAK, ...) and play.mcd, the memory card
// It sets what play.cmd sets (release settings, no diagnostics) and runs the game with game\ as working directory.
#include <windows.h>
#include <string>

static std::wstring folderOf(const std::wstring &path)
{
    return path.substr(0, path.find_last_of(L"\\/"));
}

static bool exists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t self[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, self, static_cast<DWORD>(sizeof(self) / sizeof(self[0])));
    const std::wstring root = folderOf(self);
    const std::wstring game = root + L"\\game";
    const std::wstring runner = game + L"\\ps2EntryRunner.exe";
    const std::wstring elf = root + L"\\disc\\SCUS_973.99";
    if (!exists(runner) || !exists(elf))
    {
        MessageBoxW(nullptr, (L"Missing game files next to the launcher:\n" + runner + L"\n" + elf).c_str(), L"God of War",
                    MB_ICONERROR);
        return 1;
    }

    SetEnvironmentVariableW(L"PS2X_GS_WORKER", L"1");
    SetEnvironmentVariableW(L"GOW_GS_FINISH_ASINCRONO", L"1");
    SetEnvironmentVariableW(L"GOW_AUDIO_DEVICE", L"default");
    SetEnvironmentVariableW(L"GOW_MC0", (root + L"\\disc\\play.mcd").c_str());

    std::wstring command = L"\"" + runner + L"\" \"" + elf + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(runner.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, game.c_str(), &startup, &process))
    {
        MessageBoxW(nullptr, L"Could not start game\\ps2EntryRunner.exe.", L"God of War", MB_ICONERROR);
        return 1;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
