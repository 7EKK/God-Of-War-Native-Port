// Synthetic dual-layer ISO tests for the portable first-run disc importer.
// No Sony files are used in this test.
#include "disc_import.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace
{
constexpr std::size_t sec = 2048;
void put32(char *p, std::uint32_t n)
{
    for (int i = 0; i < 4; ++i) p[i] = static_cast<char>(n >> (i * 8));
}
void check(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}
std::size_t record(char *p, const std::string &name, std::uint32_t extent, std::uint32_t bytes,
                   bool directory = false)
{
    const auto len = static_cast<unsigned>(33 + name.size() + ((33 + name.size()) & 1));
    std::memset(p, 0, len);
    p[0] = static_cast<char>(len);
    put32(p + 2, extent);
    put32(p + 10, bytes);
    p[25] = directory ? 2 : 0;
    p[32] = static_cast<char>(name.size());
    std::memcpy(p + 33, name.data(), name.size());
    return len;
}
void setPvd(char *p, std::uint32_t size, std::uint32_t root)
{
    p[0] = 1;
    std::memcpy(p + 1, "CD001", 5);
    p[6] = 1;
    put32(p + 80, size);
    record(p + 156, std::string(1, '\0'), root, sec, true);
}
void fakeIso(const fs::path &iso)
{
    std::vector<char> bytes(420 * sec, 0);
    auto at = [&](std::size_t lsn) -> char * { return bytes.data() + lsn * sec; };
    setPvd(at(16), 200, 22);
    setPvd(at(200), 220, 23); // second-layer LSN base 184, root at absolute sector 207
    std::size_t p = 0;
    p += record(at(22) + p, std::string(1, '\0'), 22, sec, true);
    p += record(at(22) + p, "SCUS_973.99;1", 30, sec);
    p += record(at(22) + p, "PART1.PAK;1", 32, sec);
    p += record(at(22) + p, "GODOFWAR.TOC;1", 34, sec);
    p = 0;
    p += record(at(207) + p, std::string(1, '\0'), 23, sec, true);
    p += record(at(207) + p, "PART2.PAK;1", 40, sec); // absolute 224
    std::memcpy(at(30), "SCUS USA executable", 19);
    std::memcpy(at(32), "PART1", 5);
    std::memcpy(at(34), "GODOFWAR.TOC", 12);
    std::memcpy(at(224), "PART2", 5);
    std::ofstream out(iso, std::ios::binary);
    check(bool(out), "Cannot create synthetic ISO.");
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    check(bool(out), "Cannot write synthetic ISO.");
}
void verifyMetadata(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary);
    check(bool(f), "Metadata missing.");
    std::array<char, sec + 4> rec{};
    bool first = false, second = false, root0 = false, root1 = false;
    while (f.read(rec.data(), rec.size()))
    {
        const auto lsn = static_cast<unsigned char>(rec[0]) |
            (static_cast<unsigned char>(rec[1]) << 8) |
            (static_cast<unsigned char>(rec[2]) << 16) |
            (static_cast<unsigned char>(rec[3]) << 24);
        if (lsn == 16) first = true;
        if (lsn == 200) second = true;
        if (lsn == 22) root0 = true;
        if (lsn == 207) root1 = true;
    }
    check(first && second && root0 && root1, "Required ISO metadata sector missing.");
}
} // namespace
int main()
{
    fs::path workspace;
    try
    {
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        workspace = fs::temp_directory_path() / ("gow-iso-import-test-" + unique);
        fs::create_directories(workspace);
        const auto iso = workspace / "disc.iso";
        const auto data = workspace / "first" / "data";
        const auto copied = workspace / "second" / "data";
        fakeIso(iso);
        gow_import::importIso(iso, data, {});
        check(gow_import::validData(data), "Extracted ISO files rejected.");
        check(fs::file_size(data / "SCUS_973.99") == sec, "USA ELF extraction failed.");
        check(fs::file_size(data / "PART2.PAK") == sec, "Second-layer PAK extraction failed.");
        verifyMetadata(data / ".disc_meta");
        gow_import::importFolder(data, copied, {});
        check(gow_import::validData(copied), "Extracted folder import failed.");
        verifyMetadata(copied / ".disc_meta");
        fs::remove(copied / ".disc_meta");
        check(!gow_import::validData(copied), "Incomplete metadata was accepted.");
        fs::path unrelated = workspace / "invalid.iso";
        { std::ofstream out(unrelated, std::ios::binary); out << "not a PlayStation disc"; }
        bool refused = false;
        try { gow_import::importIso(unrelated, workspace / "invalid" / "data", {}); }
        catch (const std::exception &) { refused = true; }
        check(refused, "Invalid ISO was accepted.");
        check(!fs::exists(workspace / "invalid" / "data"), "Failed import wrote into final destination.");
        fs::remove_all(workspace);
        std::cout << "PASS: ISO and folder imports, both layers, metadata, validation\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << "\n";
        if (!workspace.empty()) { std::error_code ec; fs::remove_all(workspace, ec); }
        return 1;
    }
}
