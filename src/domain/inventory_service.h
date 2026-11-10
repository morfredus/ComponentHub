/**
 * InventoryService - Business logic of the inventory.
 *
 * Only knows the repository interfaces (IComponentRepository,
 * ILocationRepository, IStockMovementRepository), never their implementation
 * (SQLite). Independent of the platform (Windows, Linux,
 * Raspberry Pi).
 */

#pragma once
#include <vector>
#include <optional>
#include <string>
#include "repositories.h"

namespace domain {

struct ComponentFilter {
    std::string query;      // free text search (name, reference, manufacturer, category, supplier, notes)
    std::string category;
    std::string status;     // empty = no filter (to_test, validating, validated, defective, archived)
    Id locationId = kNoId;  // kNoId = no filter
    bool onlyLowStock = false;
    bool hasKind = false;    // false = no filter on `kind`
    ComponentKind kind = ComponentKind::Component;
};

class InventoryService {
public:
    InventoryService(IComponentRepository& components,
                      ILocationRepository& locations,
                      ICategoryRepository& categories,
                      IStockMovementRepository& movements)
        : _components(components), _locations(locations), _categories(categories), _movements(movements) {}

    std::vector<Component> listComponents(const ComponentFilter& filter = {}) const;
    // Saves the component; if its `category` field is set and matches
    // no existing Category, one is created on the fly
    // (see _ensureCategoryExists) - the component record keeps a free text field,
    // while the list of managed categories always stays up to date.
    Component saveComponent(Component component);
    bool removeComponent(Id id);

    std::vector<Location> listLocations() const;
    Location saveLocation(Location location);
    bool removeLocation(Id id);

    std::vector<Category> listCategories() const;
    Category saveCategory(Category category);
    bool removeCategory(Id id);

    // Records a stock movement and updates the quantity of the component
    // concerned. Returns the component after the change, or nullopt if
    // the referenced component does not exist.
    std::optional<Component> recordMovement(StockMovement movement);
    std::vector<StockMovement> history(Id componentId) const;

    // Components under their minimum threshold - basis of the automatic shopping list.
    std::vector<Component> lowStock() const;

private:
    IComponentRepository&     _components;
    ILocationRepository&      _locations;
    ICategoryRepository&      _categories;
    IStockMovementRepository& _movements;

    static bool _matches(const Component& c, const ComponentFilter& filter);
    void _ensureCategoryExists(const std::string& name);
};

} // namespace domain
