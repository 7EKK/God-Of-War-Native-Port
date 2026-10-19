// Portable God of War (USA) disc importer. Does not require Python or any installed SDK.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace gow_import
{
namespace fs = std::filesystem;
using Progress = std::function<void(const std::wstring &stage, std::uint64_t completed, std::uint64_t total)>;

// Only the NTSC-U SCUS-97399 release is supported.
// A usable extracted folder needs .disc_layout and .disc_meta, because the game reads raw disc sectors.
bool validData(const fs::path &dir);
void importIso(const fs::path &iso, const fs::path &destination, const Progress &progress);
void importFolder(const fs::path &source, const fs::path &destination, const Progress &progress);
} // namespace gow_import
