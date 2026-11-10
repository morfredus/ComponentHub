#include "platform/Tar.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace chdesktop::tar {
namespace {

fs::path fsPath(const std::string& p) { return fs::u8path(p); }

void writeOctal(char* field, size_t width, std::uint64_t value) {
    for (size_t i = 0; i + 1 < width; i++) { field[width - 2 - i] = char('0' + (value & 7)); value >>= 3; }
    field[width - 1] = '\0';
}
std::uint64_t parseOctal(const char* p, size_t width) {
    std::uint64_t v = 0;
    for (size_t i = 0; i < width; i++) {
        char c = p[i];
        if (c >= '0' && c <= '7') v = (v << 3) + (std::uint64_t)(c - '0');
        else if (c == ' ' || c == '\0') continue;
        else break;
    }
    return v;
}

// ustar checksum: bytes of the header, with the checksum field counted as spaces.
std::uint64_t headerChecksum(const char h[512]) {
    std::uint64_t sum = 0;
    for (int i = 0; i < 512; i++) sum += (i >= 148 && i < 156) ? (unsigned char)' ' : (unsigned char)h[i];
    return sum;
}

// ustar header. `name` is already relative (no leading "/"), with a trailing "/" for a folder.
void buildHeader(char h[512], const std::string& name, std::uint64_t size, char type) {
    std::memset(h, 0, 512);
    std::memcpy(h, name.data(), std::min<size_t>(name.size(), 100));
    writeOctal(h + 100, 8, type == '5' ? 0755 : 0644);
    writeOctal(h + 108, 8, 0);
    writeOctal(h + 116, 8, 0);
    writeOctal(h + 124, 12, size);
    writeOctal(h + 136, 12, 0);
    h[156] = type;
    std::memcpy(h + 257, "ustar", 5);
    h[263] = '0'; h[264] = '0';
    writeOctal(h + 148, 7, headerChecksum(h));
    h[155] = ' ';
}

// Name of a component usable as a Windows AND Linux file: rejects ":" (NTFS alternate data
// stream, drive letter), forbidden characters, reserved names (CON, NUL,
// COM1...) with or without an extension, and names ending with a dot or a space.
bool portableName(const std::string& name) {
    if (name.empty() || name.back() == '.' || name.back() == ' ') return false;
    for (unsigned char ch : name)
        if (ch < 0x20 || std::strchr("<>:\"|?*", ch)) return false;
    std::string stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    for (auto& ch : stem) ch = (char)std::toupper((unsigned char)ch);
    static const char* const reserved[] = {"CON", "PRN", "AUX", "NUL"};
    for (const char* r : reserved) if (stem == r) return false;
    if (stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0) &&
        stem[3] >= '0' && stem[3] <= '9') return false;
    return true;
}

// Safe archive path: relative, separated by "/", no empty component, "." or "..",
// every component portable, bounded depth. A trailing "/" (folder) is tolerated.
bool safeArchivePath(std::string name, int* depth = nullptr) {
    if (!name.empty() && name.back() == '/') name.pop_back();
    if (name.empty() || name.find('\\') != std::string::npos) return false;
    int parts = 0;
    size_t start = 0;
    for (;;) {
        const size_t slash = name.find('/', start);
        const std::string comp = name.substr(start, slash == std::string::npos ? slash : slash - start);
        if (comp.empty() || comp.find("..") != std::string::npos || !portableName(comp)) return false;
        parts++;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    if (depth) *depth = parts;
    return parts <= kMaxDepth;
}

bool excluded(const std::string& name, const std::vector<std::string>& suffixes) {
    // ".tmp.": temporary file of an atomic write in progress (attachment store).
    if (name.find(".tmp.") != std::string::npos) return true;
    for (const auto& s : suffixes)
        if (name.size() >= s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) return true;
    return false;
}

struct PackEntry {
    fs::path source;      // disk path (empty for a synthetic folder)
    std::string name;     // name in the archive
    bool isDir = false;
    std::uint64_t size = 0;
};

} // namespace

bool pack(const std::string& tarPath, const std::string& dir,
          const std::vector<std::string>& excludeSuffixes, std::string* error,
          const std::vector<TreeSource>& extraTrees, std::uint64_t maxTotalBytes) {
    // A failure midway must not leave a truncated archive that would look
    // valid: the partial file is deleted.
    std::ofstream out;
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        out.close();   // Windows refuses to delete a file that is still open
        std::error_code rmEc;
        fs::remove(fsPath(tarPath), rmEc);
        return false;
    };

    // 1) Inventory and validation BEFORE writing anything: an unacceptable name or size
    //    is reported right away, with no half-written archive.
    std::vector<PackEntry> entries;
    std::uint64_t total = 0;
    const std::string selfName = fs::u8path(tarPath).filename().u8string();
    auto walk = [&](const std::string& prefix, const std::string& root, bool isMain) -> std::string {
        std::error_code ec;
        const fs::path rootPath = fsPath(root);
        if (!prefix.empty()) {
            if (!safeArchivePath(prefix)) return "tar.invalid_name|" + prefix;
            entries.push_back({{}, prefix + "/", true, 0});
        }
        fs::recursive_directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, ec), end;
        if (ec) return "tar.read_failed|" + root;
        for (; it != end; it.increment(ec)) {
            if (ec) return "tar.read_failed|" + root;
            const auto& e = *it;
            const fs::file_status st = e.symlink_status(ec);
            if (ec) return "tar.read_failed|" + e.path().u8string();
            if (fs::is_symlink(st)) { it.disable_recursion_pending(); continue; }   // never a link
            const bool isDir = fs::is_directory(st);
            if (!isDir && !fs::is_regular_file(st)) continue;
            const std::string fname = e.path().filename().u8string();
            if (!isDir && excluded(fname, excludeSuffixes)) continue;
            if (isMain && !isDir && it.depth() == 0 && fname == selfName) continue;  // never include itself

            const std::string rel = e.path().lexically_relative(rootPath).generic_u8string();
            const std::string name = (prefix.empty() ? rel : prefix + "/" + rel) + (isDir ? "/" : "");
            int depth = 0;
            if (!safeArchivePath(name, &depth)) {
                // Tells the causes apart to give a useful message.
                return std::string(depth > kMaxDepth ? "tar.too_deep|" : "tar.invalid_name|") + name;
            }
            if (name.size() > 99) return "tar.name_too_long|" + name;

            PackEntry pe{e.path(), name, isDir, 0};
            if (!isDir) {
                pe.size = (std::uint64_t)e.file_size(ec);
                if (ec) return "tar.read_failed|" + name;
                if (pe.size > kMaxFileBytes) return "tar.too_large|" + name;
                total += pe.size;
                if (total > maxTotalBytes) return "tar.too_large|" + std::to_string(maxTotalBytes);
            }
            entries.push_back(std::move(pe));
        }
        return {};
    };
    std::string err = walk({}, dir, true);
    for (const auto& t : extraTrees) {
        if (!err.empty()) break;
        std::error_code ec;
        if (!fs::is_directory(fsPath(t.dir), ec)) continue;   // tree missing: nothing to archive
        err = walk(t.archiveName, t.dir, false);
    }
    if (!err.empty()) { if (error) *error = err; return false; }
    // Stable order: a folder always precedes its content and the archive is reproducible.
    std::sort(entries.begin(), entries.end(),
              [](const PackEntry& a, const PackEntry& b) { return a.name < b.name; });

    // 2) Writing.
    out.open(fsPath(tarPath), std::ios::binary | std::ios::trunc);
    if (!out) return fail("tar.write_failed|" + tarPath);
    char buf[4096];
    for (const auto& pe : entries) {
        char header[512];
        buildHeader(header, pe.name, pe.size, pe.isDir ? '5' : '0');
        out.write(header, 512);
        if (!pe.isDir) {
            std::ifstream in(pe.source, std::ios::binary);
            if (!in) return fail("tar.read_failed|" + pe.name);
            std::uint64_t remaining = pe.size;
            while (remaining > 0) {
                const std::streamsize chunk = (std::streamsize)std::min<std::uint64_t>(remaining, sizeof(buf));
                in.read(buf, chunk);
                const std::streamsize got = in.gcount();
                if (got <= 0) break;
                out.write(buf, got);
                remaining -= (std::uint64_t)got;
            }
            // File shorter than expected (modified while being read): the header announces a
            // size that the data does not match, the archive would be corrupted.
            if (remaining != 0) return fail("tar.read_failed|" + pe.name);
            const size_t pad = (512 - (size_t)(pe.size % 512)) % 512;
            if (pad) { std::memset(buf, 0, pad); out.write(buf, (std::streamsize)pad); }
        }
        if (!out) return fail("tar.write_failed|" + tarPath);
    }
    std::memset(buf, 0, 512);
    out.write(buf, 512);
    out.write(buf, 512);
    out.flush();
    return out ? true : fail("tar.write_failed|" + tarPath);
}

bool looksLikeTar(const std::string& tarPath) {
    std::ifstream in(fsPath(tarPath), std::ios::binary);
    if (!in) return false;
    char magic[6] = {0};
    in.seekg(257);
    in.read(magic, 5);
    return in.gcount() == 5 && std::strncmp(magic, "ustar", 5) == 0;
}

int extract(const std::string& tarPath, const std::string& dir, std::vector<std::string>& errors) {
    std::ifstream in(fsPath(tarPath), std::ios::binary);
    if (!in) return 0;

    int restored = 0;
    bool sawEnd = false;
    char header[512];
    char buf[4096];
    for (;;) {
        in.read(header, 512);
        const std::streamsize got = in.gcount();
        if (got == 0) break;                                         // end of file without an end marker
        if (got != 512) { errors.push_back("tar.corrupt"); return restored; }  // truncated header
        bool allZero = true;
        for (int i = 0; i < 512; i++) if (header[i]) { allZero = false; break; }
        if (allZero) { sawEnd = true; break; }

        // Checksum: rejects a file that is not (or no longer) a valid tar.
        const std::uint64_t stored = parseOctal(header + 148, 8);
        std::uint64_t signedSum = 0;
        for (int i = 0; i < 512; i++)
            signedSum += (i >= 148 && i < 156) ? (std::uint64_t)' ' : (std::uint64_t)(int)(signed char)header[i];
        if (stored != headerChecksum(header) && stored != signedSum) { errors.push_back("tar.corrupt"); return restored; }

        char nameBuf[101];
        std::memcpy(nameBuf, header, 100);
        nameBuf[100] = '\0';
        std::string name(nameBuf);
        const std::uint64_t size = parseOctal(header + 124, 12);
        const char type = header[156];
        const bool isDir = (type == '5');
        const bool isFile = (type == '0' || type == '\0');
        const std::uint64_t pad = (512 - (size % 512)) % 512;
        // A 100-byte name fills the whole field: a non-empty ustar prefix (long path)
        // is not handled, hence refused rather than restored under another name.
        const bool hasPrefix = header[345] != '\0';

        const bool safe = (isFile || isDir) && !hasPrefix && safeArchivePath(name);
        if (!safe) {
            in.seekg((std::streamoff)(size + pad), std::ios::cur);
            errors.push_back("tar.unsafe_entry|" + name);
            continue;
        }

        if (isDir) {
            std::error_code ec;
            fs::create_directories(fsPath(dir) / fs::u8path(name), ec);
            if (ec) errors.push_back("tar.write_failed|" + name);
            if (size + pad) in.seekg((std::streamoff)(size + pad), std::ios::cur);
            continue;
        }

        const fs::path target = fsPath(dir) / fs::u8path(name);
        std::error_code mkEc;
        fs::create_directories(target.parent_path(), mkEc);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        bool ok = (bool)out && !mkEc;
        std::uint64_t remaining = size;
        bool truncated = false;
        while (remaining > 0) {
            const std::streamsize chunk = (std::streamsize)std::min<std::uint64_t>(remaining, sizeof(buf));
            in.read(buf, chunk);
            const std::streamsize n = in.gcount();
            // Size actually read: a truncated archive must not pass for a complete one.
            if (n != chunk) { ok = false; truncated = true; break; }
            if (out) out.write(buf, n);
            remaining -= (std::uint64_t)n;
        }
        if (ok) { out.flush(); ok = (bool)out; }
        out.close();
        if (pad && in && !truncated) in.seekg((std::streamoff)pad, std::ios::cur);
        if (ok) restored++;
        else {
            // Partial file deleted, and reading stopped if the stream is already finished.
            std::error_code rmEc;
            fs::remove(target, rmEc);
            errors.push_back("tar.write_failed|" + name);
            if (truncated || !in) return restored;
        }
    }
    // Without the two final null blocks, the archive was cut between two entries.
    if (!sawEnd) errors.push_back("tar.corrupt");
    return restored;
}

} // namespace chdesktop::tar
