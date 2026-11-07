// Utilitaires communs aux tests ctest (sans framework) : CHECK, dossiers temporaires, fichiers.
#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
inline int g_failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); g_failures++; } \
    } while (0)

inline fs::path freshDir(const char* name) {
    fs::path d = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d);
    return d;
}

inline void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream o(p, std::ios::binary);
    o << content;
}
inline std::string readFile(const fs::path& p) {
    std::ifstream i(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(i), {});
}
