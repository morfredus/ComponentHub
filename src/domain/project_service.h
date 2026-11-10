/**
 * ProjectService - Business logic of projects: bill of materials (BOM) and
 * computation of the missing components compared with the current stock.
 *
 * Depends on IComponentRepository (read only) to know the available
 * quantities, but never modifies the inventory - the
 * responsibility for stock movements stays with InventoryService.
 */

#pragma once
#include <vector>
#include <optional>
#include <string>
#include "project_repositories.h"
#include "repositories.h"

namespace domain {

// Joined view (BOM + availability): not a persisted entity.
struct ProjectComponentView {
    Id id = kNoId;              // id of the ProjectComponent line
    Id componentId = kNoId;     // kNoId = off-inventory item
    std::string componentName;  // component name, or free label if off-inventory
    std::string reference;      // reference if in inventory, otherwise empty
    bool inInventory = false;   // false = off-inventory item (always "to buy")
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

    // componentId == kNoId + non-empty label: off-inventory item (to buy).
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

private:
    IProjectRepository&          _projects;
    IProjectComponentRepository& _projectComponents;
    IComponentRepository&        _components;
};

} // namespace domain
