// A dependency-free ISO9660 dual-layer extractor for the retail God of War NTSC-U disc.
// Mirrors tools/iso_extract.py: preserves LSNs in .disc_layout and non-file sectors in .disc_meta.
// No Windows dependencies: this component is also exercised on Linux with synthetic ISO fixtures.
#include "disc_import.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gow_import
{
namespace
{
constexpr std::uint64_t sector = 2048;
struct Entry
{
    std::string name;
    std::uint64_t lsn, size;
};
struct Table
{
    std::vector<Entry> files;
    std::uint64_t sectors = 0, layer1 = 0;
};
std::uint32_t u32(const char *p)
{
    const auto *b = reinterpret_cast<const unsigned char *>(p);
    return std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
           (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
}
void fail(const char *message) { throw std::runtime_error(message); }
std::string upper(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
bool safeName(const std::string &s)
{
    if (s.empty() || s == "." || s == ".." || s.back() == '.' || s.back() == ' ')
        return false;
    for (unsigned char c : s)
        if (c < 32 || c >= 127 || c == '/' || c == '\\' || c == ':' ||
            c == '<' || c == '>' || c == '|' || c == '*' || c == '?' || c == '"')
            return false;
    const std::string u = upper(s.substr(0, s.find('.')));
    return u != "CON" && u != "PRN" && u != "AUX" && u != "NUL" &&
           u != "COM1" && u != "COM2" && u != "COM3" && u != "COM4" &&
           u != "LPT1" && u != "LPT2" && u != "LPT3" && u != "LPT4";
}
bool safeRelative(const std::string &name)
{
    if (name.empty() || name.front() == '/' || name.front() == '\\') return false;
    std::size_t at = 0;
    while (at < name.size())
    {
        const auto pos = name.find('/', at);
        const auto len = (pos == std::string::npos ? name.size() : pos) - at;
        if (!safeName(name.substr(at, len))) return false;
        if (pos == std::string::npos) return true;
        at = pos + 1;
    }
    return false;
}
std::uint64_t length(std::ifstream &in)
{
    in.clear();
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0) fail("Cannot determine disc image size.");
    return static_cast<std::uint64_t>(end);
}
void readAt(std::ifstream &in, std::uint64_t at, char *dst, std::size_t count)
{
    if (at > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        fail("Disc offset is too large.");
    in.clear();
    in.seekg(static_cast<std::streamoff>(at));
    if (!in.read(dst, static_cast<std::streamsize>(count)))
        fail("Disc image is truncated or unreadable.");
}
bool pvd(const char *b) { return b[0] == 1 && std::memcmp(b + 1, "CD001", 5) == 0; }
void readDirectory(std::ifstream &in, std::uint64_t lsn, std::uint64_t size,
                   std::uint64_t base, const std::string &prefix, Table &t, unsigned depth)
{
    if (depth > 8 || size > 32 * 1024 * 1024) fail("Invalid ISO directory tree.");
    const std::uint64_t absolute = base + lsn;
    if (absolute >= t.sectors || size > (t.sectors - absolute) * sector)
        fail("ISO directory points outside the image.");
    std::vector<char> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) readAt(in, absolute * sector, bytes.data(), bytes.size());
    std::size_t pos = 0;
    while (pos < bytes.size())
    {
        const unsigned n = static_cast<unsigned char>(bytes[pos]);
        if (!n)
        {
            pos = ((pos / sector) + 1) * sector;
            continue;
        }
        if (n < 34 || n > bytes.size() - pos || (pos % sector) + n > sector)
            fail("Malformed ISO directory record.");
        const char *record = bytes.data() + pos;
        const unsigned nameBytes = static_cast<unsigned char>(record[32]);
        if (33 + nameBytes > n) fail("Malformed ISO filename.");
        const std::string raw(record + 33, record + 33 + nameBytes);
        pos += n;
        if (raw.size() == 1 && (raw[0] == '\0' || raw[0] == '\1')) continue;
        std::string name = raw.substr(0, raw.find(';'));
        if (!safeName(name)) fail("Unsafe filename in ISO.");
        const std::string full = prefix.empty() ? name : prefix + "/" + name;
        const auto extent = u32(record + 2);
        const auto bytesCount = u32(record + 10);
        if ((record[25] & 2) != 0)
            readDirectory(in, extent, bytesCount, base, full, t, depth + 1);
        else
        {
            const std::uint64_t first = base + extent;
            if (first >= t.sectors || bytesCount > (t.sectors - first) * sector)
                fail("ISO file extends past the disc image.");
            t.files.push_back({full, first, bytesCount});
        }
    }
}
Table inspectIso(std::ifstream &in)
{
    const auto size = length(in);
    if (size < 20 * sector || size % sector != 0)
        fail("Not a valid sector-aligned PlayStation 2 ISO.");
    Table t;
    t.sectors = size / sector;
    std::array<char, sector> desc{};
    readAt(in, 16 * sector, desc.data(), desc.size());
    if (!pvd(desc.data())) fail("ISO9660 primary volume descriptor not found.");
    const auto layer0 = u32(desc.data() + 80);
    if (layer0 < 32 || layer0 > t.sectors) fail("Invalid disc layer size.");
    const auto root = desc.data() + 156;
    readDirectory(in, u32(root + 2), u32(root + 10), 0, "", t, 0);
    if (layer0 < t.sectors)
    {
        readAt(in, std::uint64_t(layer0) * sector, desc.data(), desc.size());
        if (pvd(desc.data()))
        {
            t.layer1 = layer0 - 16;
            const auto root2 = desc.data() + 156;
            readDirectory(in, u32(root2 + 2), u32(root2 + 10), t.layer1, "", t, 0);
        }
    }
    return t;
}
bool mandatory(const Table &t)
{
    bool elf = false, part1 = false, part2 = false, toc = false;
    for (const auto &f : t.files)
    {
        const auto n = upper(f.name);
        elf   |= (n == "SCUS_973.99" && f.size > 0);
        part1 |= (n == "PART1.PAK" && f.size > 0);
        part2 |= (n == "PART2.PAK" && f.size > 0);
        toc   |= (n == "GODOFWAR.TOC" && f.size > 0);
    }
    return elf && part1 && part2 && toc && t.layer1 != 0;
}
void requireGame(const Table &t)
{
    if (!mandatory(t))
        fail("Unsupported disc. Use God of War (USA), SCUS-97399, with both DVD layers.");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> occupied;
    for (const auto &f : t.files)
    {
        if (!safeRelative(f.name)) fail("Unsafe disc path.");
        occupied.push_back({f.lsn, f.lsn + (f.size + sector - 1) / sector});
    }
    std::sort(occupied.begin(), occupied.end());
    for (std::size_t i = 1; i < occupied.size(); ++i)
        if (occupied[i].first < occupied[i - 1].second)
            fail("Overlapping ISO files in disc layout.");
}
Table readLayout(const fs::path &dir)
{
    std::ifstream in(dir / ".disc_layout");
    if (!in) fail("Missing .disc_layout. Import an ISO or a folder extracted with --layout.");
    Table t;
    std::string line;
    bool hasSectors = false, hasLayer = false;
    while (std::getline(in, line))
    {
        std::istringstream row(line);
        std::string tag;
        row >> tag;
        if (tag == "sectors")
        {
            if (!(row >> t.sectors)) fail("Invalid .disc_layout sector count.");
            hasSectors = true;
        }
        else if (tag == "layer1")
        {
            if (!(row >> t.layer1)) fail("Invalid .disc_layout layer boundary.");
            hasLayer = true;
        }
        else if (tag == "file")
        {
            Entry e;
            if (!(row >> e.lsn >> e.size >> e.name) || !safeRelative(e.name))
                fail("Invalid filename or entry in .disc_layout.");
            t.files.push_back(e);
        }
        else if (!tag.empty()) fail("Unknown .disc_layout record.");
    }
    if (!hasSectors || !hasLayer || t.sectors == 0) fail("Incomplete .disc_layout.");
    requireGame(t);
    for (const auto &e : t.files)
    {
        if (e.lsn >= t.sectors || (e.size + sector - 1) / sector > t.sectors - e.lsn)
            fail("File in .disc_layout is outside the disc.");
        if (!fs::is_regular_file(dir / fs::u8path(e.name)) ||
            fs::file_size(dir / fs::u8path(e.name)) != e.size)
            fail("Missing or incorrectly sized extracted game file.");
    }
    if (!fs::is_regular_file(dir / ".disc_meta") ||
        fs::file_size(dir / ".disc_meta") % (sector + 4) != 0)
        fail("Missing or invalid .disc_meta file.");
    return t;
}
void copyBytes(std::ifstream &in, std::ofstream &out, std::uint64_t size,
               const Progress &progress, const std::wstring &label,
               std::uint64_t &done, std::uint64_t total)
{
    std::array<char, 1024 * 1024> buffer{};
    while (size)
    {
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), size));
        if (!in.read(buffer.data(), static_cast<std::streamsize>(n))) fail("Cannot read game data.");
        if (!out.write(buffer.data(), static_cast<std::streamsize>(n))) fail("Cannot write extracted game data.");
        size -= n;
        done += n;
        if (progress) progress(label, done, total);
    }
}
void fileCopy(const fs::path &src, const fs::path &dst,
              const Progress &progress, std::uint64_t &done, std::uint64_t total)
{
    fs::create_directories(dst.parent_path());
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!in || !out) fail("Cannot open a game file during import.");
    copyBytes(in, out, fs::file_size(src), progress, src.filename().wstring(), done, total);
    out.close();
    if (!out) fail("Could not finalize an extracted game file.");
}
struct Stage
{
    fs::path dst, tmp;
    bool committed = false;
    explicit Stage(const fs::path &p) : dst(p), tmp(p.wstring() + L".importing")
    {
        if (fs::exists(tmp)) fail("Previous import staging folder exists. Check and remove data.importing before retrying.");
        if (fs::exists(dst) && (!fs::is_directory(dst) || !fs::is_empty(dst)))
            fail("The data folder exists but is incomplete. Move it aside before importing again.");
        fs::create_directories(tmp);
    }
    void commit()
    {
        if (!validData(tmp)) fail("Imported data did not pass verification.");
        if (fs::exists(dst)) fs::remove(dst); // Only permitted if it was empty at construction.
        fs::rename(tmp, dst);
        committed = true;
    }
    ~Stage()
    {
        if (!committed)
        {
            std::error_code ec;
            fs::remove_all(tmp, ec);
        }
    }
};
} // namespace

bool validData(const fs::path &dir)
{
    try { (void)readLayout(dir); return true; }
    catch (...) { return false; }
}
void importFolder(const fs::path &source, const fs::path &destination, const Progress &progress)
{
    Table t = readLayout(source); // Validate fully BEFORE writing.
    if (fs::exists(destination) && fs::equivalent(source, destination)) return;
    Stage stage(destination);
    std::uint64_t total = fs::file_size(source / ".disc_layout") + fs::file_size(source / ".disc_meta");
    for (const auto &e : t.files) total += e.size;
    std::uint64_t done = 0;
    for (const auto &e : t.files)
        fileCopy(source / fs::u8path(e.name), stage.tmp / fs::u8path(e.name), progress, done, total);
    fileCopy(source / ".disc_layout", stage.tmp / ".disc_layout", progress, done, total);
    fileCopy(source / ".disc_meta", stage.tmp / ".disc_meta", progress, done, total);
    stage.commit();
}
void importIso(const fs::path &iso, const fs::path &destination, const Progress &progress)
{
    std::ifstream in(iso, std::ios::binary);
    if (!in) fail("Cannot open the selected ISO.");
    Table t = inspectIso(in);
    requireGame(t); // Validate before creating output.
    Stage stage(destination);
    std::sort(t.files.begin(), t.files.end(), [](const Entry &a, const Entry &b) { return a.lsn < b.lsn; });
    std::uint64_t fileBytes = 0, coveredSectors = 0;
    for (const auto &f : t.files)
    {
        fileBytes += f.size;
        coveredSectors += (f.size + sector - 1) / sector;
    }
    const auto total = fileBytes + (t.sectors - coveredSectors) * sector;
    std::uint64_t done = 0;
    for (const auto &e : t.files)
    {
        const fs::path target = stage.tmp / fs::u8path(e.name);
        fs::create_directories(target.parent_path());
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) fail("Cannot create an extracted game file.");
        in.clear();
        in.seekg(static_cast<std::streamoff>(e.lsn * sector));
        copyBytes(in, out, e.size, progress, fs::u8path(e.name).filename().wstring(), done, total);
        out.close();
        if (!out) fail("Could not finalize ISO file extraction.");
    }
    {
        std::ofstream layout(stage.tmp / ".disc_layout", std::ios::binary);
        if (!layout) fail("Cannot write .disc_layout.");
        layout << "sectors " << t.sectors << "\nlayer1 " << t.layer1 << "\n";
        for (const auto &e : t.files)
            layout << "file " << e.lsn << ' ' << e.size << ' ' << e.name << "\n";
        if (!layout) fail("Cannot finish .disc_layout.");
    }
    // Raw CDVD reads need metadata for all non-file sectors, not merely the loose game files.
    std::ofstream meta(stage.tmp / ".disc_meta", std::ios::binary);
    if (!meta) fail("Cannot create .disc_meta.");
    std::array<char, sector * 256> buf{};
    std::uint64_t cursor = 0;
    for (std::size_t i = 0; i <= t.files.size(); ++i)
    {
        const std::uint64_t start = (i < t.files.size() ? t.files[i].lsn : t.sectors);
        if (start < cursor) fail("Overlapping disc file ranges.");
        std::uint64_t gap = start - cursor;
        in.clear();
        in.seekg(static_cast<std::streamoff>(cursor * sector));
        while (gap)
        {
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(gap, 256));
            if (!in.read(buf.data(), static_cast<std::streamsize>(n * sector)))
                fail("Cannot read ISO metadata sectors.");
            for (std::size_t k = 0; k < n; ++k)
            {
                const char *p = buf.data() + k * sector;
                if (std::any_of(p, p + sector, [](char c) { return c != 0; }))
                {
                    const std::uint32_t lsn = static_cast<std::uint32_t>(cursor + k);
                    const char bytes[4] = {static_cast<char>(lsn), static_cast<char>(lsn >> 8),
                                           static_cast<char>(lsn >> 16), static_cast<char>(lsn >> 24)};
                    meta.write(bytes, 4);
                    meta.write(p, sector);
                    if (!meta) fail("Cannot write disc metadata.");
                }
            }
            cursor += n;
            gap -= n;
            done += n * sector;
            if (progress) progress(L"Preparing disc sector layout", done, total);
        }
        if (i < t.files.size())
            cursor = t.files[i].lsn + (t.files[i].size + sector - 1) / sector;
    }
    meta.close();
    if (!meta) fail("Cannot finish .disc_meta.");
    stage.commit();
}
} // namespace gow_import
