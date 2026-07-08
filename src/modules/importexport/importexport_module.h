/**
 * ImportExportModule — Page /import-export + API CSV (natif et Bomist).
 *
 * Utilise ses propres instances de ComponentJsonRepository/
 * LocationJsonRepository (mêmes fichiers LittleFS qu'InventoryModule) : ces
 * dépôts sont sans état, donc plusieurs instances restent cohérentes.
 */

#pragma once
#include "../../core/module.h"
#include "../../domain/import_export_service.h"
#include "../../storage/component_repository.h"
#include "../../storage/location_repository.h"

class ImportExportModule : public Module {
public:
    const char* name() const override { return "ImportExport"; }
    void registerRoutes(WebRouter& router) override;

private:
    ComponentJsonRepository     _componentRepo;
    LocationJsonRepository      _locationRepo;
    domain::ImportExportService _service{_componentRepo, _locationRepo};

    void _registerPages(WebRouter& router);
    void _registerApiRoutes(WebRouter& router);
};
