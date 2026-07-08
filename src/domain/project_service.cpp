#include "project_service.h"

namespace domain {

std::vector<ProjectComponentView> ProjectService::bom(Id projectId) const {
    std::vector<ProjectComponentView> out;
    for (auto& link : _projectComponents.findByProject(projectId)) {
        ProjectComponentView view;
        view.id = link.id;
        view.componentId = link.componentId;
        view.quantityRequired = link.quantityRequired;

        if (link.componentId == kNoId) {
            // Élément hors inventaire : rien en stock, tout est à acheter.
            view.componentName = link.label.empty() ? "(élément sans nom)" : link.label;
            view.inInventory = false;
            view.quantityAvailable = 0;
        } else if (auto comp = _components.findById(link.componentId)) {
            view.componentName = comp->name;
            view.reference = comp->reference;
            view.inInventory = true;
            view.unitPrice = comp->price;
            view.quantityAvailable = comp->quantity;
        } else {
            // Le composant a été supprimé de l'inventaire depuis son ajout.
            view.componentName = "(composant supprimé)";
            view.inInventory = false;
            view.quantityAvailable = 0;
        }

        view.quantityMissing = view.quantityRequired > view.quantityAvailable
            ? view.quantityRequired - view.quantityAvailable : 0;
        out.push_back(view);
    }
    return out;
}

std::vector<ProjectComponentView> ProjectService::missingComponents(Id projectId) const {
    std::vector<ProjectComponentView> out;
    for (auto& view : bom(projectId)) {
        if (view.quantityMissing > 0) out.push_back(view);
    }
    return out;
}

} // namespace domain
