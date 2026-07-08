/**
 * ProjectService — Logique métier des projets : nomenclature (BOM) et
 * calcul des composants manquants par rapport au stock courant.
 *
 * Dépend d'IComponentRepository (lecture seule) pour connaître les
 * quantités disponibles, mais ne modifie jamais l'inventaire — la
 * responsabilité des mouvements de stock reste à InventoryService.
 */

#pragma once
#include <vector>
#include <optional>
#include <string>
#include "project_repositories.h"
#include "repositories.h"

namespace domain {

// Vue jointe (BOM + disponibilité) : pas une entité persistée.
struct ProjectComponentView {
    Id id = kNoId;              // id de la ligne ProjectComponent
    Id componentId = kNoId;     // kNoId = élément hors inventaire
    std::string componentName;  // nom du composant, ou libellé libre si hors inventaire
    std::string reference;      // référence si en inventaire, sinon vide
    bool inInventory = false;   // false = élément hors inventaire (toujours « à acheter »)
    double unitPrice = 0.0;     // prix unitaire connu (inventaire), sinon 0
    int quantityRequired = 0;
    int quantityAvailable = 0;
    int quantityMissing = 0;    // max(0, quantityRequired - quantityAvailable)
};

class ProjectService {
public:
    ProjectService(IProjectRepository& projects,
                    IProjectComponentRepository& projectComponents,
                    IComponentRepository& components)
        : _projects(projects), _projectComponents(projectComponents), _components(components) {}

    std::vector<Project> listProjects() const { return _projects.findAll(); }
    std::optional<Project> getProject(Id id) const { return _projects.findById(id); }
    Project saveProject(Project project) { return _projects.save(std::move(project)); }

    bool removeProject(Id id) {
        _projectComponents.removeByProject(id);
        return _projects.remove(id);
    }

    // componentId == kNoId + label non vide : élément hors inventaire (à acheter).
    ProjectComponent addComponent(Id projectId, Id componentId, const std::string& label, int quantityRequired) {
        ProjectComponent link;
        link.projectId = projectId;
        link.componentId = componentId;
        link.label = (componentId == kNoId) ? label : std::string();
        link.quantityRequired = quantityRequired;
        return _projectComponents.save(link);
    }

    bool removeComponent(Id linkId) { return _projectComponents.remove(linkId); }

    std::vector<ProjectComponentView> bom(Id projectId) const;
    std::vector<ProjectComponentView> missingComponents(Id projectId) const;

private:
    IProjectRepository&          _projects;
    IProjectComponentRepository& _projectComponents;
    IComponentRepository&        _components;
};

} // namespace domain
