#include "platform/Tar.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace chdesktop::tar {
namespace {

fs::path fsPath(const std::string& p) { return fs::u8path(p); }

void writeOctal(char* field, size_t width, unsigned long value) {
    for (size_t i = 0; i + 1 < width; i++) { field[width - 2 - i] = char('0' + (value & 7)); value >>= 3; }
    field[width - 1] = '\0';
}
unsigned long parseOctal(const char* p, size_t width) {
    unsigned long v = 0;
    for (size_t i = 0; i < width; i++) {
        char c = p[i];
        if (c >= '0' && c <= '7') v = (v << 3) + (unsigned long)(c - '0');
        else if (c == ' ' || c == '\0') continue;
        else break;
    }
    return v;
}
// En-tête ustar « fichier normal ». `name` est déjà relatif (sans « / »).
void buildHeader(char h[512], const std::string& name, unsigned long size) {
    std::memset(h, 0, 512);
    std::strncpy(h, name.c_str(), 99);
    writeOctal(h + 100, 8, 0644);
    writeOctal(h + 108, 8, 0);
    writeOctal(h + 116, 8, 0);
    writeOctal(h + 124, 12, size);
    writeOctal(h + 136, 12, 0);
    std::memset(h + 148, ' ', 8);
    h[156] = '0';
    std::memcpy(h + 257, "ustar", 5);
    h[263] = '0'; h[264] = '0';
    unsigned int sum = 0;
    for (int i = 0; i < 512; i++) sum += (unsigned char)h[i];
    writeOctal(h + 148, 7, sum);
    h[155] = ' ';
}

bool excluded(const std::string& name, const std::vector<std::string>& suffixes) {
    for (const auto& s : suffixes)
        if (name.size() >= s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) return true;
    return false;
}

} // namespace

bool pack(const std::string& tarPath, const std::string& dir,
          const std::vector<std::string>& excludeSuffixes) {
    std::ofstream out(fsPath(tarPath), std::ios::binary | std::ios::trunc);
    if (!out) return false;

    char buf[512];
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fsPath(dir), ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().u8string();
        if (excluded(name, excludeSuffixes)) continue;
        if (name == fs::u8path(tarPath).filename().u8string()) continue;  // jamais s'inclure soi-même

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) continue;
        const unsigned long size = (unsigned long)entry.file_size();

        char header[512];
        buildHeader(header, name, size);
        out.write(header, 512);

        unsigned long remaining = size;
        while (remaining > 0) {
            const std::streamsize chunk = (std::streamsize)std::min<unsigned long>(remaining, sizeof(buf));
            in.read(buf, chunk);
            const std::streamsize got = in.gcount();
            if (got <= 0) break;
            out.write(buf, got);
            remaining -= (unsigned long)got;
        }
        const size_t pad = (512 - (size_t)(size % 512)) % 512;
        if (pad) { std::memset(buf, 0, pad); out.write(buf, (std::streamsize)pad); }
        if (!out) return false;
    }
    std::memset(buf, 0, 512);
    out.write(buf, 512);
    out.write(buf, 512);
    return (bool)out;
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
    char header[512];
    char buf[512];
    while (in.read(header, 512) && in.gcount() == 512) {
        bool allZero = true;
        for (int i = 0; i < 512; i++) if (header[i]) { allZero = false; break; }
        if (allZero) break;

        char nameBuf[101];
        std::memcpy(nameBuf, header, 100);
        nameBuf[100] = '\0';
        std::string name(nameBuf);
        const unsigned long size = parseOctal(header + 124, 12);
        const char type = header[156];
        const size_t pad = (512 - (size_t)(size % 512)) % 512;

        // Sécurité : fichier normal, nom non vide, sans remontée de dossier.
        const bool safe = (type == '0' || type == '\0') && !name.empty() &&
                          name.find("..") == std::string::npos &&
                          name.find('/') == std::string::npos && name.find('\\') == std::string::npos;
        if (!safe) {
            in.seekg((std::streamoff)(size + pad), std::ios::cur);
            errors.push_back("entrée ignorée : " + name);
            continue;
        }

        const fs::path target = fsPath(dir) / fs::u8path(name);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        bool ok = (bool)out;
        unsigned long remaining = size;
        while (remaining > 0) {
            const std::streamsize chunk = (std::streamsize)std::min<unsigned long>(remaining, sizeof(buf));
            in.read(buf, chunk);
            const std::streamsize got = in.gcount();
            if (got <= 0) { ok = false; break; }
            if (out) out.write(buf, got);
            remaining -= (unsigned long)got;
        }
        if (pad) in.seekg((std::streamoff)pad, std::ios::cur);
        if (ok && out) restored++;
        else errors.push_back("échec d'écriture : " + name);
    }
    return restored;
}

} // namespace chdesktop::tar
