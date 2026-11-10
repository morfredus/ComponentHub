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

// Checks an extracted database: opening, integrity_check, components table. Opening in
// read/write applies any -wal inherited from an old backup
// (raw copy) along the way; the TRUNCATE checkpoint merges it into the main file.
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

// Undo stack: if a step of the replacement fails, the previous ones are undone in
// reverse order, so as to never leave a new database with old attachments (or
// l'inverse).
class UndoStack {
public:
    // Undoes the step "from was renamed to to" by putting `to` back under the name `from`.
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
    // Guaranteed cleanup, whatever the exit path.
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code e; fs::remove_all(p, e); } } cleanup{staging};

    try {
        db.snapshotTo((staging / kDbName).u8string());
    } catch (const std::exception& e) {
        error = std::string("backup.snapshot_failed|") + e.what();
        return false;
    }

    // Other files: plain copy, WITHOUT the database (replaced by the snapshot) nor its
    // -wal/-shm (intermediate state, inconsistent outside the database) nor the .bak files (safety copies of
    // a previous restore, which would double the archive size). A legacy app_config.json
    // (token included) is discarded: the configuration comes from the
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

    // Attachments: archived in place (no intermediate copy, they can be
    // large). A missing folder is simply ignored by pack.
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
        // Truncated or corrupted archive: fatal. Dangerous entry skipped: simple warning.
        if (e.rfind("tar.write_failed", 0) == 0 || e.rfind("tar.corrupt", 0) == 0) return fail(e);
        warnings.push_back(e);
    }

    const fs::path dbFile = pending / kDbName;
    if (!fs::is_regular_file(dbFile, ec)) return fail("backup.no_database");
    std::string vErr;
    if (!validateDb(dbFile, vErr)) return fail(vErr);
    removeWalFiles(dbFile);   // merged by the checkpoint: must not follow the database

    // Flat files that must never overwrite existing ones: the .bak files (a 1.11 archive
    // contains some, and overwriting the local .bak would destroy the safety copy just made),
    // the -wal/-shm files and temporary files. A file named "attachments" would not replace the folder.
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

    // Attachments: keep only files named after their SHA-256 AND whose content
    // matches (this is the store contract). A doubtful file is dropped with a warning:
    // the blob is downloaded again from the hub at the next synchronization, the hash reference staying valid.
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

    // Each step depends on its source existing: after an abrupt stop
    // midway, the next start resumes where the previous one stopped. A .bak
    // is only deleted if the current data still exists (so that .bak is an old
    // level): otherwise it IS the only copy of the old data.
    UndoStack undo;
    auto failWith = [&](const fs::path& what) {
        undo.rollback();
        error = "backup.apply_failed|" + what.u8string();
        return false;
    };

    // 1) Set the current data aside (nothing is deleted before success).
    if (haveDb && fs::exists(target, ec)) {
        fs::remove(bak, ec);
        removeWalFiles(target);   // an orphan -wal would be replayed on the NEW database
        fs::rename(target, bak, ec);
        if (ec) return failWith(target);
        undo.push(target, bak);
    }
    if (haveAtt && fs::exists(attTarget, ec)) {
        fs::remove_all(attBak, ec);   // old level: the current one becomes the new .bak
        fs::rename(attTarget, attBak, ec);
        if (ec) return failWith(attTarget);
        undo.push(attTarget, attBak);
    }
    // 2) Put the restored data in place.
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

    // Other flat files (sync_state.json...): replaced one by one.
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
