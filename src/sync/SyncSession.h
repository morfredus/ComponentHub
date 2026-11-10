/**
 * SyncSession - everything ONE thread needs to synchronize: its own SQLite connection,
 * its own repository instances and its own attachment store.
 *
 * Why: a SQLite connection and the repositories' prepared statements cannot be shared
 * between threads. Synchronization runs in a dedicated worker thread; it therefore opens its session
 * ("per thread" factory) on the same database in WAL mode. The user's writes (connection
 * of the interface) and those of the synchronization alternate thanks to busy_timeout and
 * short transactions (one PULL page at a time).
 *
 * Header only, no Qt: testable with ctest.
 */
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "attachment_store.h"
#include "storage/SQLiteRepositories.h"
#include "sync_record.h"

namespace chsync {

class SyncSession {
public:
    // `dir`: data folder (the one of AppContext). Throws std::runtime_error if the database
    // cannot be opened.
    explicit SyncSession(const std::string& dir)
        : dir_(dir)
        , db_(dir + "/componenthub.sqlite")
        , ready_(db_.openExisting())
        , components_(db_.handle())
        , locations_(db_.handle())
        , categories_(db_.handle())
        , documents_(db_.handle())
        , projects_(db_.handle())
        , projectComponents_(db_.handle())
        , referentials_(db_.handle())
        , attachments_(dir + "/attachments") {}

    // Same list, same order as AppContext::syncables().
    std::vector<domain::ISyncableRepository*> syncables() {
        return { &components_, &locations_, &categories_, &documents_,
                 &projects_, &projectComponents_, &referentials_ };
    }
    domain::AttachmentStore& attachments() { return attachments_; }
    chdesktop::SQLiteDatabase& database() { return db_; }
    std::string statePath() const { return dir_ + "/sync_state.json"; }

private:
    // Declaration order = construction order: the database before the repositories that read it.
    std::string dir_;
    chdesktop::SQLiteDatabase db_;
    bool ready_;
    chdesktop::SQLiteComponentRepository        components_;
    chdesktop::SQLiteLocationRepository         locations_;
    chdesktop::SQLiteCategoryRepository         categories_;
    chdesktop::SQLiteDocumentRepository         documents_;
    chdesktop::SQLiteProjectRepository          projects_;
    chdesktop::SQLiteProjectComponentRepository projectComponents_;
    chdesktop::SQLiteReferentialRepository      referentials_;
    domain::AttachmentStore                     attachments_;
};

} // namespace chsync
