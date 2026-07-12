#include "inventory_service.h"
#include <algorithm>
#include <cctype>

namespace domain {

static std::string _toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool InventoryService::_matches(const Component& c, const ComponentFilter& filter) {
    if (filter.onlyLowStock && !(c.minStock > 0 && c.quantity < c.minStock)) return false;
    if (filter.locationId != kNoId && c.locationId != filter.locationId) return false;
    if (!filter.category.empty() && c.category != filter.category) return false;
    if (!filter.status.empty() && c.status != filter.status) return false;
    if (filter.hasKind && c.kind != filter.kind) return false;

    if (!filter.query.empty()) {
        // Recherche « universelle » : l'utilisateur n'a pas à savoir dans quel
        // champ se trouve l'info. On balaie TOUS les champs texte indexables du
        // composant (référence, désignation, type, fabricant, catégorie,
        // caractéristiques, fournisseur, notes...). Chaque mot de la requête
        // doit être trouvé quelque part (recherche multi-termes, ET logique).
        const std::string haystack = _toLower(
            c.name + " " + c.reference + " " + c.manufacturer + " " + c.description + " " +
            c.category + " " + c.subcategory + " " + c.type + " " + c.voltage + " " +
            c.current + " " + c.interfaceType + " " + c.protocols + " " + c.i2cAddress + " " +
            c.frequency + " " + c.compatibility + " " + c.supplier + " " + c.state + " " +
            c.origin + " " + c.warranty + " " + c.notes);

        std::string q = _toLower(filter.query);
        size_t start = 0;
        while (start < q.size()) {
            size_t end = q.find(' ', start);
            if (end == std::string::npos) end = q.size();
            if (end > start) {
                const std::string term = q.substr(start, end - start);
                if (haystack.find(term) == std::string::npos) return false;
            }
            start = end + 1;
        }
    }
    return true;
}

std::vector<Component> InventoryService::listComponents(const ComponentFilter& filter) const {
    std::vector<Component> out;
    for (auto& c : _components.findAll()) {
        if (_matches(c, filter)) out.push_back(c);
    }
    return out;
}

std::optional<Component> InventoryService::getComponent(Id id) const {
    return _components.findById(id);
}

Component InventoryService::saveComponent(Component component) {
    _ensureCategoryExists(component.category);
    return _components.save(std::move(component));
}

bool InventoryService::removeComponent(Id id) {
    return _components.remove(id);
}

std::vector<Location> InventoryService::listLocations() const {
    return _locations.findAll();
}

Location InventoryService::saveLocation(Location location) {
    return _locations.save(std::move(location));
}

bool InventoryService::removeLocation(Id id) {
    return _locations.remove(id);
}

std::vector<Category> InventoryService::listCategories() const {
    return _categories.findAll();
}

Category InventoryService::saveCategory(Category category) {
    return _categories.save(std::move(category));
}

bool InventoryService::removeCategory(Id id) {
    return _categories.remove(id);
}

void InventoryService::_ensureCategoryExists(const std::string& name) {
    if (name.empty()) return;
    for (auto& existing : _categories.findAll()) {
        if (existing.name == name) return;
    }
    Category cat;
    cat.name = name;
    _categories.save(cat);
}

std::optional<Component> InventoryService::recordMovement(StockMovement movement) {
    auto existing = _components.findById(movement.componentId);
    if (!existing) return std::nullopt;

    Component component = *existing;
    switch (movement.type) {
        case StockMovementType::In:         component.quantity += movement.quantity; break;
        case StockMovementType::Out:        component.quantity -= movement.quantity; break;
        case StockMovementType::Correction:
        case StockMovementType::Inventory:  component.quantity  = movement.quantity; break;
    }
    if (component.quantity < 0) component.quantity = 0;

    _movements.add(movement);
    return _components.save(component);
}

std::vector<StockMovement> InventoryService::history(Id componentId) const {
    return _movements.findByComponent(componentId);
}

std::vector<Component> InventoryService::lowStock() const {
    std::vector<Component> out;
    for (auto& c : _components.findAll()) {
        if (c.minStock > 0 && c.quantity < c.minStock) out.push_back(c);
    }
    return out;
}

} // namespace domain
