/**
 * attachment_store.h - Local, content-addressed store for attachments.
 *
 * A "file" attachment (as opposed to a plain http link) is copied here,
 * into a file named after the SHA-256 of its content. The Document only
 * keeps the hash: the identity of the content is therefore stable across
 * machines, and two documents with identical content share a single file (deduplication).
 *
 * This is the local counterpart of the morfSync blob store: same names (the hash),
 * so synchronizing = transferring the files missing on one side or the
 * other. See docs/sync-contract.md §4.5.
 *
 * Header only, without Qt (the core is independent of it): uses only the
 * standard library and chutil::sha256Hex.
 */

#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <atomic>
#include <cctype>
#include <iterator>
#include <cstdint>
#include "util/sha256.h"

namespace domain {

class AttachmentStore {
public:
    explicit AttachmentStore(std::string dir)
        : dir_(std::filesystem::u8path(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir_, ec);
    }

    static bool validHash(const std::string& h) {
        if (h.size() != 64) return false;
        for (char c : h)
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    bool has(const std::string& hash) const {
        if (!validHash(hash)) return false;
        std::error_code ec;
        return std::filesystem::exists(path(hash), ec);
    }

    // Path of the content file (named after the hash, no extension). Used by
    // the UI to open the attachment.
    std::string filePath(const std::string& hash) const {
        return path(hash).string();
    }

    bool get(const std::string& hash, std::string& out) const {
        std::ifstream in(path(hash), std::ios::binary);
        if (!in) return false;
        out.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
        return true;
    }

    // Writes a received blob (e.g. downloaded from the hub) under its hash. Idempotent,
    // atomic write. Does NOT enforce that the hash matches the content:
    // the caller (synchronization) checks integrity beforehand.
    bool put(const std::string& hash, const std::string& bytes) {
        if (!validHash(hash)) return false;
        if (has(hash)) return true;
        const std::filesystem::path target = path(hash);
        const std::filesystem::path tmp =
            std::filesystem::path(target.string() + ".tmp." + std::to_string(nextTmp()));
        {
            std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
            if (!o) return false;
            o.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            o.flush();
            if (!o) { std::error_code rm; std::filesystem::remove(tmp, rm); return false; }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) { std::error_code rm; std::filesystem::remove(tmp, rm); return has(hash); }
        return true;
    }

    struct ImportResult {
        bool         ok = false;
        std::string  hash;         // SHA-256 of the content
        std::int64_t sizeBytes = 0;
    };

    // Copies a file from disk into the store, under the hash of its content.
    // Returns the hash and the size; the content now lives in the store,
    // independent of the original file (which can be moved or deleted).
    ImportResult importFile(const std::string& srcPath) {
        ImportResult r;
        std::ifstream in(std::filesystem::u8path(srcPath), std::ios::binary);
        if (!in) return r;
        std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        r.hash = chutil::sha256Hex(bytes);
        r.sizeBytes = static_cast<std::int64_t>(bytes.size());
        r.ok = put(r.hash, bytes);
        return r;
    }

private:
    std::filesystem::path path(const std::string& hash) const { return dir_ / hash; }

    static long long nextTmp() {
        static std::atomic<long long> n{0};
        return ++n;
    }

    std::filesystem::path dir_;
};

} // namespace domain
