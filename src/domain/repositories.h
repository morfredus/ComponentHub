/**
 * Repository interfaces - the only access door to persistence
 * as seen from the domain. No implementation here: see src/storage/
 * (SQLite today). Changing the storage engine only requires
 * re-implementing these interfaces, without touching InventoryService.
 */

#pragma once
#include <vector>
#include <optional>
#include "component.h"
#include "location.h"
#include "category.h"
#include "stock_movement.h"

namespace domain {

class IComponentRepository {
public:
    virtual ~IComponentRepository() = default;
    virtual std::vector<Component> findAll() const = 0;
    virtual std::optional<Component> findById(Id id) const = 0;
    virtual Component save(Component component) = 0;  // id==kNoId -> creation, otherwise update
    // Batch upsert: a single disk access for the whole batch, instead of one per
    // item. Essential for bulk imports (see ImportExportService):
    // calling save() in a loop reads/rewrites the whole file for each item,
    // a cost that explodes with the number of lines (already observed: watchdog
    // reset after ~150 s for an import of 39 lines before this fix).
    virtual std::vector<Component> saveAll(const std::vector<Component>& components) = 0;
    virtual bool remove(Id id) = 0;
};

class ILocationRepository {
public:
    virtual ~ILocationRepository() = default;
    virtual std::vector<Location> findAll() const = 0;
    virtual std::optional<Location> findById(Id id) const = 0;
    virtual Location save(Location location) = 0;
    virtual bool remove(Id id) = 0;
};

class ICategoryRepository {
public:
    virtual ~ICategoryRepository() = default;
    virtual std::vector<Category> findAll() const = 0;
    virtual std::optional<Category> findById(Id id) const = 0;
    virtual Category save(Category category) = 0;
    virtual bool remove(Id id) = 0;
};

class IStockMovementRepository {
public:
    virtual ~IStockMovementRepository() = default;
    virtual std::vector<StockMovement> findByComponent(Id componentId) const = 0;
    virtual StockMovement add(StockMovement movement) = 0;
};

} // namespace domain
