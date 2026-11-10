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
            // Off-inventory item: nothing in stock, everything is to be bought.
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
            // The component was removed from the inventory after it was added.
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

} // namespace domain
