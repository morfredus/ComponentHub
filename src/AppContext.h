// ComponentHub Desktop - AppContext SQLite.
// Le stockage est uniquement SQLite : componenthub.sqlite dans le dossier de données.
// Le schéma est créé automatiquement au premier lancement.
#pragma once

#include <string>
#include <memory>
#include <stdexcept>

#include "storage/SQLiteRepositories.h"
#include "inventory_service.h"
#include "project_service.h"
#include "document_service.h"
#include "import_export_service.h"
#include "sync_record.h"
#include "attachment_store.h"

namespace chdesktop {

class AppContext {
private:
    // Ces deux membres doivent être déclarés avant les dépôts : C++ construit
    // les membres dans leur ordre de déclaration, pas dans celui de la liste
    // d'initialisation du constructeur.
    std::string _dir;
    std::unique_ptr<SQLiteDatabase> _db;

public:
    explicit AppContext(std::string dir)
        : _dir(std::move(dir))
        , _db(_openDb(_dir))
        , components(_db->handle())
        , locations(_db->handle())
        , categories(_db->handle())
        , movements(_db->handle())
        , documents(_db->handle())
        , projects(_db->handle())
        , projectComponents(_db->handle())
        , referentials(_db->handle())
        , attachments(_dir + "/attachments")
        , inventory(components, locations, categories, movements)
        , projects_service(projects, projectComponents, components)
        , documents_service(documents)
        , importExport(components, locations, categories, projects)
    {}

    const std::string& dir() const { return _dir; }

    std::vector<domain::ISyncableRepository*> syncables() {
        return { &components, &locations, &categories, &documents,
                 &projects, &projectComponents, &referentials };
    }

    SQLiteComponentRepository        components;
    SQLiteLocationRepository         locations;
    SQLiteCategoryRepository         categories;
    SQLiteStockMovementRepository    movements;
    SQLiteDocumentRepository         documents;
    SQLiteProjectRepository          projects;
    SQLiteProjectComponentRepository projectComponents;
    SQLiteReferentialRepository      referentials;

    domain::AttachmentStore     attachments;
    domain::InventoryService    inventory;
    domain::ProjectService      projects_service;
    domain::DocumentService     documents_service;
    domain::ImportExportService importExport;

private:
    // Ouvre (ou crée) la base et initialise le schéma. Lance une exception si
    // la base ne peut pas être ouverte.
    static std::unique_ptr<SQLiteDatabase> _openDb(const std::string& dir) {
        auto db = std::make_unique<SQLiteDatabase>(dir + "/componenthub.sqlite");
        if (!db->isOpen()) {
            throw std::runtime_error("sqlite_open_failed:" + dir + "/componenthub.sqlite");
        }
        if (!db->initialize()) {
            throw std::runtime_error("sqlite_initialize_failed:" + dir + "/componenthub.sqlite");
        }
        return db;
    }
};

} // namespace chdesktop
