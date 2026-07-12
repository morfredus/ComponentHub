// AppContext — assemble les dépôts fichier et les services métier du domaine
// pour un répertoire de données donné. C'est l'unique point d'accès aux
// données pour l'interface : chaque écran reçoit une référence à AppContext.
//
// Les services (InventoryService, ProjectService, DocumentService,
// ImportExportService) sont ceux de src/domain/, compilés tels quels : toute
// la logique métier est partagée avec la version ESP32.
#pragma once

#include <string>

#include "storage/FileRepositories.h"
#include "inventory_service.h"
#include "project_service.h"
#include "document_service.h"
#include "import_export_service.h"

namespace chdesktop {

class AppContext {
public:
    explicit AppContext(std::string dir)
        : components(dir + "/inventory_components.json"),
          locations(dir + "/inventory_locations.json"),
          categories(dir + "/inventory_categories.json"),
          movements(dir + "/inventory_stock_movements.json"),
          documents(dir + "/inventory_documents.json"),
          projects(dir + "/projects.json"),
          projectComponents(dir + "/project_components.json"),
          referentials(dir + "/referentials.json"),
          inventory(components, locations, categories, movements),
          projects_service(projects, projectComponents, components),
          documents_service(documents),
          importExport(components, locations, categories, projects),
          _dir(std::move(dir)) {}

    const std::string& dir() const { return _dir; }

    // Dépôts (déclarés avant les services qui les référencent).
    ComponentRepository        components;
    LocationRepository         locations;
    CategoryRepository         categories;
    StockMovementRepository    movements;
    DocumentRepository         documents;
    ProjectRepository          projects;
    ProjectComponentRepository projectComponents;
    ReferentialRepository      referentials;

    // Services métier partagés (src/domain/).
    domain::InventoryService    inventory;
    domain::ProjectService      projects_service;
    domain::DocumentService     documents_service;
    domain::ImportExportService importExport;

private:
    std::string _dir;
};

} // namespace chdesktop
