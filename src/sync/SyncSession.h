/**
 * SyncSession - tout ce dont UN thread a besoin pour synchroniser : sa propre connexion SQLite,
 * ses propres instances de dépôts et son propre magasin de pièces jointes.
 *
 * Pourquoi : une connexion SQLite et les statements préparés des dépôts ne se partagent pas
 * entre threads. La synchro tourne dans un thread de travail dédié ; elle ouvre donc sa session
 * (fabrique « par thread ») sur la même base en WAL. Les écritures de l'utilisateur (connexion
 * de l'interface) et celles de la synchro s'alternent grâce à busy_timeout et à des
 * transactions courtes (une page du PULL à la fois).
 *
 * En-tête seul, sans Qt : testable en ctest.
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
    // `dir` : dossier de données (celui de l'AppContext). Lève std::runtime_error si la base
    // ne s'ouvre pas.
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

    // Même liste, même ordre que AppContext::syncables().
    std::vector<domain::ISyncableRepository*> syncables() {
        return { &components_, &locations_, &categories_, &documents_,
                 &projects_, &projectComponents_, &referentials_ };
    }
    domain::AttachmentStore& attachments() { return attachments_; }
    chdesktop::SQLiteDatabase& database() { return db_; }
    std::string statePath() const { return dir_ + "/sync_state.json"; }

private:
    // Ordre de déclaration = ordre de construction : la base avant les dépôts qui la lisent.
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
