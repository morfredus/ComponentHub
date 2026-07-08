/**
 * ProjectModule — Page /projects + API REST /api/project(s) : fiche projet,
 * nomenclature de composants et calcul des composants manquants.
 *
 * Utilise sa propre instance de ComponentJsonRepository (même fichier
 * /inventory_components.json qu'InventoryModule) pour lire les quantités
 * disponibles : ces dépôts sont sans état (chaque appel relit LittleFS), donc
 * deux instances restent cohérentes sans état partagé entre modules.
 */

#pragma once
#include "../../core/module.h"
#include "../../domain/project_service.h"
#include "../../storage/project_repository.h"
#include "../../storage/project_component_repository.h"
#include "../../storage/component_repository.h"

class ProjectModule : public Module {
public:
    const char* name() const override { return "Project"; }
    void registerRoutes(WebRouter& router) override;

private:
    ProjectJsonRepository          _projectRepo;
    ProjectComponentJsonRepository _projectComponentRepo;
    ComponentJsonRepository        _componentRepo;
    domain::ProjectService         _service{_projectRepo, _projectComponentRepo, _componentRepo};

    void _registerPages(WebRouter& router);
    void _registerProjectRoutes(WebRouter& router);
    void _registerBomRoutes(WebRouter& router);
};
