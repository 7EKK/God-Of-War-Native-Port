// First-run game-data import UI. Win32 only; the actual disc parser is portable C++.
// Supports ISO9660 dual-layer God of War USA images and fully extracted discs with
// .disc_layout + .disc_meta. No Python, compiler, web access or external applications.
#include "import_ui.h"
#include "disc_import.h"

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <vector>

namespace
{
namespace fs = std::filesystem;

bool chooseIso(const std::wstring &root, fs::path &result)
{
    wchar_t filename[32768] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"PlayStation 2 ISO (*.iso)\0*.iso\0All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename) / sizeof(filename[0]);
    ofn.lpstrInitialDir = root.c_str();
    ofn.lpstrTitle = L"Select your own God of War (USA) PS2 ISO";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    result = filename;
    return true;
}

bool chooseFolder(fs::path &result)
{
    BROWSEINFOW browse{};
    browse.lpszTitle = L"Choose extracted God of War (USA) folder (.disc_layout and .disc_meta required)";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&browse);
    if (!id) return false;
    wchar_t path[MAX_PATH] = {};
    const bool ok = SHGetPathFromIDListW(id, path) != FALSE;
    CoTaskMemFree(id);
    if (ok) result = path;
    return ok;
}

fs::path adjacentIso(const fs::path &root)
{
    fs::path found;
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec)) continue;
        const std::wstring ext = it->path().extension().wstring();
        if (_wcsicmp(ext.c_str(), L".iso") != 0) continue;
        if (!found.empty()) return {}; // Two candidates: let the user decide.
        found = it->path();
    }
    return found;
}

class ProgressWindow
{
    HWND window_ = nullptr, label_ = nullptr, bar_ = nullptr;
    ULONGLONG lastUpdate_ = 0;
public:
    ProgressWindow()
    {
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&icc);
        window_ = CreateWindowExW(WS_EX_APPWINDOW, L"STATIC", L"God of War - Importing",
                                  WS_POPUP | WS_CAPTION | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                  470, 130, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window_) throw std::runtime_error("Could not create the import progress window.");
        label_ = CreateWindowExW(0, L"STATIC", L"Preparing game data...", WS_CHILD | WS_VISIBLE,
                                18, 15, 432, 40, window_, nullptr, GetModuleHandleW(nullptr), nullptr);
        bar_ = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                              18, 66, 432, 22, window_, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!label_ || !bar_) throw std::runtime_error("Could not initialize the import progress window.");
        SendMessageW(label_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(bar_, PBM_SETRANGE32, 0, 1000);
        const int x = (GetSystemMetrics(SM_CXSCREEN) - 470) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - 130) / 2;
        SetWindowPos(window_, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        UpdateWindow(window_);
    }
    ~ProgressWindow() { if (window_) DestroyWindow(window_); }
    void update(const std::wstring &stage, std::uint64_t done, std::uint64_t total)
    {
        const auto now = GetTickCount64();
        if (done < total && now - lastUpdate_ < 85) return;
        lastUpdate_ = now;
        const auto pct = total ? std::min<std::uint64_t>(1000, done * 1000 / total) : 0;
        const std::wstring status = stage + L"\n" +
            std::to_wstring(done / (1024 * 1024)) + L" / " +
            std::to_wstring(total / (1024 * 1024)) + L" MiB";
        SetWindowTextW(label_, status.c_str());
        SendMessageW(bar_, PBM_SETPOS, static_cast<WPARAM>(pct), 0);
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
};

std::wstring asciiError(const std::exception &e)
{
    // Error messages raised by disc_import.cpp are intentionally ASCII.
    const std::string s = e.what();
    return std::wstring(s.begin(), s.end());
}
} // namespace

bool ensureGameData(const std::wstring &root)
{
    const fs::path installRoot(root);
    const fs::path data = installRoot / "data";
    if (gow_import::validData(data)) return true;

    if (fs::exists(data) && (!fs::is_directory(data) || !fs::is_empty(data)))
    {
        MessageBoxW(nullptr,
            L"The data folder already contains incomplete or unsupported files.\n\n"
            L"Move it aside before importing a new disc. Existing saves and settings will not be changed.",
            L"God of War - Data import", MB_ICONERROR);
        return false;
    }

    const fs::path nearby = adjacentIso(installRoot);
    fs::path selected;
    bool useIso = false;
    if (!nearby.empty())
    {
        const std::wstring message = L"Found a PS2 ISO next to the launcher:\n" +
            nearby.filename().wstring() +
            L"\n\nImport this ISO now?\n\nYes: use this ISO\nNo: choose another source\nCancel: exit";
        const int result = MessageBoxW(nullptr, message.c_str(), L"God of War - First run",
                                       MB_YESNOCANCEL | MB_ICONQUESTION);
        if (result == IDCANCEL) return false;
        if (result == IDYES) { selected = nearby; useIso = true; }
    }
    if (selected.empty())
    {
        const int result = MessageBoxW(nullptr,
            L"God of War needs your own NTSC-U (SCUS-97399) game data.\n\n"
            L"YES - Select your PS2 ISO (extract automatically)\n"
            L"NO - Select previously extracted game files (must include .disc_layout and .disc_meta)\n"
            L"CANCEL - Exit\n\nNo separate Python or developer tools are required.",
            L"God of War - Import game data", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (result == IDCANCEL) return false;
        useIso = result == IDYES;
        if (useIso ? !chooseIso(root, selected) : !chooseFolder(selected)) return false;
    }

    try
    {
        ProgressWindow window;
        auto progress = [&window](const std::wstring &s, std::uint64_t current, std::uint64_t total)
        {
            window.update(s, current, total);
        };
        if (useIso)
            gow_import::importIso(selected, data, progress);
        else
            gow_import::importFolder(selected, data, progress);
    }
    catch (const std::exception &e)
    {
        const std::wstring message = L"Game-data import failed:\n\n" + asciiError(e) +
                                     L"\n\nYour original source was not changed. Try importing again.";
        MessageBoxW(nullptr, message.c_str(), L"God of War - Import failed", MB_ICONERROR);
        return false;
    }

    MessageBoxW(nullptr, L"Game data imported successfully.\n\nThe original ISO or source folder can be kept as a backup.",
                L"God of War - Import complete", MB_ICONINFORMATION);
    return true;
}
