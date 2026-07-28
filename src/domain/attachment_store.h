/**
 * attachment_store.h — Magasin local des pièces jointes, adressé par contenu.
 *
 * Une pièce jointe « fichier » (par opposition à un simple lien http) est copiée
 * ici, dans un fichier nommé par le SHA-256 de son contenu. Le Document n'en
 * garde que le hash : l'identité du contenu est donc stable entre postes, et deux
 * documents au contenu identique partagent un seul fichier (dédoublonnage).
 *
 * C'est le pendant local du magasin de blobs de morfSync : mêmes noms (le hash),
 * si bien que synchroniser = transférer les fichiers absents d'un côté ou de
 * l'autre. Voir docs/sync-contract.md §4.5.
 *
 * En-tête seul, sans Qt (le cœur en est indépendant) : n'utilise que la
 * bibliothèque standard et chutil::sha256Hex.
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

    // Chemin du fichier de contenu (nommé par le hash, sans extension). Utile à
    // l'UI pour ouvrir la pièce jointe.
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

    // Écrit un blob reçu (ex. téléchargé du hub) sous son hash. Idempotent,
    // écriture atomique. N'impose PAS que le hash corresponde au contenu :
    // l'appelant (la synchro) vérifie l'intégrité en amont.
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
        std::string  hash;         // SHA-256 du contenu
        std::int64_t sizeBytes = 0;
    };

    // Copie un fichier du disque dans le magasin, sous le hash de son contenu.
    // Renvoie le hash et la taille ; le contenu vit désormais dans le magasin,
    // indépendant du fichier d'origine (qui peut être déplacé ou supprimé).
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
