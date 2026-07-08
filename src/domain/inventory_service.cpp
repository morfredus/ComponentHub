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
        std::string q = _toLower(filter.query);
        auto contains = [&](const std::string& field) {
            return _toLower(field).find(q) != std::string::npos;
        };
        if (!(contains(c.name) || contains(c.reference) || contains(c.manufacturer) ||
              contains(c.category) || contains(c.supplier) || contains(c.notes))) {
            return false;
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
