#include "platform/Backup.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sqlite3.h>

#include "attachment_store.h"
#include "platform/Tar.h"
#include "util/sha256.h"

namespace fs = std::filesystem;

namespace chdesktop::backup {
namespace {

fs::path P(const std::string& s) { return fs::u8path(s); }

bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Vérifie une base extraite : ouverture, integrity_check, table components. L'ouverture en
// lecture/écriture applique au passage un éventuel -wal hérité d'une ancienne sauvegarde
// (copie brute) ; le checkpoint TRUNCATE le fusionne dans le fichier principal.
bool validateDb(const fs::path& dbFile, std::string& error) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbFile.u8string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        error = "backup.invalid_database|open";
        sqlite3_close(db);
        return false;
    }
    bool ok = false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &st, nullptr) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(st, 0);
        ok = t && std::string(reinterpret_cast<const char*>(t)) == "ok";
    }
    sqlite3_finalize(st);
    if (ok) {
        st = nullptr;
        ok = sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='components';",
                                -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW;
        sqlite3_finalize(st);
    }
    if (ok) sqlite3_exec(db, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    if (!ok) error = "backup.invalid_database|integrity";
    return ok;
}

void removeWalFiles(const fs::path& dbFile) {
    std::error_code ec;
    fs::remove(fs::path(dbFile.u8string() + "-wal"), ec);
    fs::remove(fs::path(dbFile.u8string() + "-shm"), ec);
}

// Pile d'annulations : si une étape du remplacement échoue, les précédentes sont défaites dans
// l'ordre inverse, pour ne jamais laisser une base neuve avec d'anciennes pièces jointes (ou
// l'inverse).
class UndoStack {
public:
    // Défait l'étape « from a été renommé en to » en remettant `to` sous le nom `from`.
    void push(fs::path from, fs::path to) { steps_.push_back({std::move(from), std::move(to)}); }
    void rollback() {
        for (auto it = steps_.rbegin(); it != steps_.rend(); ++it) {
            std::error_code e;
            fs::rename(it->second, it->first, e);
        }
        steps_.clear();
    }
private:
    std::vector<std::pair<fs::path, fs::path>> steps_;
};

} // namespace

bool create(SQLiteDatabase& db, const std::string& dataDir, const std::string& tarPath,
            const std::map<std::string, std::string>& extraFiles, std::string& error) {
    std::error_code ec;
    const fs::path staging = P(dataDir) / ".backup-staging";
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec) { error = "backup.staging_failed|" + staging.u8string(); return false; }
    // Nettoyage garanti, quelle que soit la sortie.
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code e; fs::remove_all(p, e); } } cleanup{staging};

    try {
        db.snapshotTo((staging / kDbName).u8string());
    } catch (const std::exception& e) {
        error = std::string("backup.snapshot_failed|") + e.what();
        return false;
    }

    // Autres fichiers : copie simple, SANS la base (remplacée par l'instantané) ni ses
    // -wal/-shm (état intermédiaire, incohérent hors de la base) ni les .bak (copies de
    // sécurité d'une restauration précédente, qui doubleraient l'archive). app_config.json
    // hérité d'une ancienne version (jeton compris) est écarté : la configuration vient de
    // `extraFiles`.
    for (const auto& entry : fs::directory_iterator(P(dataDir), ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().u8string();
        if (name == kDbName || name == "app_config.json" || endsWith(name, "-wal") ||
            endsWith(name, "-shm") || endsWith(name, ".tmp") || endsWith(name, ".bak")) continue;
        std::error_code cp;
        fs::copy_file(entry.path(), staging / entry.path().filename(), fs::copy_options::overwrite_existing, cp);
        if (cp) { error = "backup.copy_failed|" + name; return false; }
    }
    for (const auto& [name, content] : extraFiles) {
        std::ofstream out(staging / P(name), std::ios::binary | std::ios::trunc);
        out.write(content.data(), (std::streamsize)content.size());
        out.close();
        if (!out) { error = "backup.copy_failed|" + name; return false; }
    }

    // Pièces jointes : archivées en place (pas de copie intermédiaire, elles peuvent être
    // volumineuses). Un dossier absent est simplement ignoré par pack.
    std::string tarErr;
    const std::vector<tar::TreeSource> trees{{kAttachmentsDir, (P(dataDir) / kAttachmentsDir).u8string()}};
    if (!tar::pack(tarPath, staging.u8string(), {".tmp"}, &tarErr, trees)) {
        error = tarErr.empty() ? "tar.write_failed|" + tarPath : tarErr;
        return false;
    }
    return true;
}

bool stageRestore(const std::string& tarPath, const std::string& dataDir,
                  std::vector<std::string>& warnings, std::string& error) {
    if (!tar::looksLikeTar(tarPath)) { error = "backup.not_a_backup"; return false; }

    std::error_code ec;
    const fs::path pending = P(dataDir) / kPendingDir;
    fs::remove_all(pending, ec);
    fs::create_directories(pending, ec);
    if (ec) { error = "backup.staging_failed|" + pending.u8string(); return false; }

    auto fail = [&](const std::string& why) {
        error = why;
        std::error_code e;
        fs::remove_all(pending, e);
        return false;
    };

    std::vector<std::string> errs;
    tar::extract(tarPath, pending.u8string(), errs);
    for (const auto& e : errs) {
        // Archive tronquée ou corrompue : fatal. Entrée dangereuse ignorée : simple alerte.
        if (e.rfind("tar.write_failed", 0) == 0 || e.rfind("tar.corrupt", 0) == 0) return fail(e);
        warnings.push_back(e);
    }

    const fs::path dbFile = pending / kDbName;
    if (!fs::is_regular_file(dbFile, ec)) return fail("backup.no_database");
    std::string vErr;
    if (!validateDb(dbFile, vErr)) return fail(vErr);
    removeWalFiles(dbFile);   // fusionnés par le checkpoint : ne doivent pas suivre la base

    // Fichiers à plat qui ne doivent jamais écraser l'existant : les .bak (une archive 1.11 en
    // contient, et écraser le .bak local détruirait la copie de sécurité qu'on vient de faire),
    // les -wal/-shm et les temporaires. Un fichier nommé « attachments » ne remplacerait pas le dossier.
    {
        std::vector<fs::path> junk;
        for (const auto& entry : fs::directory_iterator(pending, ec)) {
            const std::string n = entry.path().filename().u8string();
            if ((entry.is_regular_file() && (endsWith(n, ".bak") || endsWith(n, "-wal") ||
                                             endsWith(n, "-shm") || endsWith(n, ".tmp"))) ||
                (n == kAttachmentsDir && !entry.is_directory()))
                junk.push_back(entry.path());
        }
        for (const auto& j : junk) {
            std::error_code e;
            fs::remove(j, e);
            warnings.push_back("tar.unsafe_entry|" + j.filename().u8string());
        }
    }

    // Pièces jointes : ne garder que les fichiers nommés par leur SHA-256 ET dont le contenu
    // correspond (c'est le contrat du magasin). Un fichier douteux est écarté avec une alerte :
    // le blob se retélécharge du hub à la prochaine synchro, la référence de hash restant valide.
    const fs::path att = pending / kAttachmentsDir;
    if (fs::is_directory(att, ec)) {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(att, ec))
            if (!entry.is_directory()) files.push_back(entry.path());
        for (const auto& f : files) {
            const std::string fname = f.filename().u8string();
            bool good = f.parent_path() == att && domain::AttachmentStore::validHash(fname);
            if (good) {
                std::ifstream in(f, std::ios::binary);
                const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                good = in.is_open() && chutil::sha256Hex(bytes) == fname;
            }
            if (!good) {
                std::error_code e;
                fs::remove(f, e);
                warnings.push_back("backup.attachment_invalid|" + fname);
            }
        }
    }
    return true;
}

bool applyPendingRestore(const std::string& dataDir, bool& applied, std::string& error) {
    applied = false;
    std::error_code ec;
    const fs::path pending = P(dataDir) / kPendingDir;
    if (!fs::is_directory(pending, ec)) return true;

    const fs::path target = P(dataDir) / kDbName;
    const fs::path bak = fs::path(target.u8string() + ".bak");
    const fs::path stagedDb = pending / kDbName;
    const fs::path attTarget = P(dataDir) / kAttachmentsDir;
    const fs::path attBak = fs::path(attTarget.u8string() + ".bak");
    const fs::path stagedAtt = pending / kAttachmentsDir;
    const bool haveDb = fs::is_regular_file(stagedDb, ec);
    const bool haveAtt = fs::is_directory(stagedAtt, ec);

    // Chaque étape est conditionnée par l'existence de sa source : après un arrêt brutal en
    // cours de route, le démarrage suivant reprend là où le précédent s'était arrêté. Un .bak
    // n'est supprimé que si la donnée courante existe encore (donc que ce .bak est un ancien
    // niveau) : sinon il EST la seule copie de l'ancienne donnée.
    UndoStack undo;
    auto failWith = [&](const fs::path& what) {
        undo.rollback();
        error = "backup.apply_failed|" + what.u8string();
        return false;
    };

    // 1) Mise de côté des données courantes (rien n'est supprimé avant le succès).
    if (haveDb && fs::exists(target, ec)) {
        fs::remove(bak, ec);
        removeWalFiles(target);   // un -wal orphelin serait rejoué sur la NOUVELLE base
        fs::rename(target, bak, ec);
        if (ec) return failWith(target);
        undo.push(target, bak);
    }
    if (haveAtt && fs::exists(attTarget, ec)) {
        fs::remove_all(attBak, ec);   // ancien niveau : l'actuel devient le nouveau .bak
        fs::rename(attTarget, attBak, ec);
        if (ec) return failWith(attTarget);
        undo.push(attTarget, attBak);
    }
    // 2) Mise en place des données restaurées.
    if (haveDb) {
        fs::rename(stagedDb, target, ec);
        if (ec) return failWith(target);
        undo.push(stagedDb, target);
    }
    if (haveAtt) {
        fs::rename(stagedAtt, attTarget, ec);
        if (ec) return failWith(attTarget);
        undo.push(stagedAtt, attTarget);
    }
    applied = haveDb || haveAtt;

    // Autres fichiers à plat (sync_state.json...) : remplacés un à un.
    std::vector<fs::path> others;
    for (const auto& entry : fs::directory_iterator(pending, ec))
        if (entry.is_regular_file()) others.push_back(entry.path());
    for (const auto& src : others) {
        const fs::path dest = P(dataDir) / src.filename();
        std::error_code e;
        fs::remove(dest, e);
        fs::rename(src, dest, e);
        if (e) { error = "backup.apply_failed|" + dest.u8string(); return false; }
    }
    fs::remove_all(pending, ec);
    return true;
}

} // namespace chdesktop::backup
