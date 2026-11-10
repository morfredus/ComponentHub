// Full backup / restore of the data folder (SQLite database + auxiliary files).
//
// Why this module: the database runs in WAL mode and stays open during the backup. Copying the
// raw file (and its -wal/-shm) gives an inconsistent state, and overwriting an open database during
// a restore silently corrupts it. Here:
//   - the backup archives a SNAPSHOT (VACUUM INTO), never the -wal/-shm files;
//   - the restore takes two steps: validation + queuing (database still open),
//     then replacement at the next start, before the database is opened.
// No Qt: testable with ctest.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "storage/SQLiteRepositories.h"

namespace chdesktop::backup {

// Name of the database file in the data folder and in the archive.
inline constexpr const char* kDbName = "componenthub.sqlite";
// Subfolder where a validated restore waits for the next start.
inline constexpr const char* kPendingDir = "restore-pending";
// Subfolder of the attachments (content-addressed store), in the data folder and
// in the archive.
inline constexpr const char* kAttachmentsDir = "attachments";

// Creates the archive `tarPath`: a consistent snapshot of the database + other regular files of the
// folder (except old .bak files) + `extraFiles` (name -> content, e.g. the configuration) + the
// attachments/ folder (recursive). `error` receives a code "xxx|detail" on failure.
bool create(SQLiteDatabase& db, const std::string& dataDir, const std::string& tarPath,
            const std::map<std::string, std::string>& extraFiles, std::string& error);

// Extracts `tarPath` into <dataDir>/restore-pending and validates the database it contains (presence,
// integrity_check, components table) and the attachments (name = SHA-256 of the content, otherwise
// dropped with a warning). Does NOT touch the current data. `warnings` receives the ignored
// entries. On failure, the waiting folder is deleted.
bool stageRestore(const std::string& tarPath, const std::string& dataDir,
                  std::vector<std::string>& warnings, std::string& error);

// To call at startup BEFORE opening the database: applies a pending restore
// (previous database kept as .bak, -wal/-shm removed; previous attachments/ set aside as
// attachments.bak, never deleted before success, put back if a step fails). An
// archive without attachments/ (1.11 and earlier) leaves the local attachments untouched.
// Returns true if there was nothing to do or everything went well; `applied` tells whether a
// remplacement a eu lieu.
bool applyPendingRestore(const std::string& dataDir, bool& applied, std::string& error);

} // namespace chdesktop::backup
