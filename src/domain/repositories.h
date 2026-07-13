/**
 * Interfaces de dépôt (Repository) — seule porte d'accès à la persistance
 * vue depuis le domaine. Aucune implémentation ici : voir src/storage/
 * (fichiers JSON). Changer de moteur de stockage (ex. SQLite) ne demande que
 * de ré-implémenter ces interfaces, sans toucher à InventoryService.
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
    virtual Component save(Component component) = 0;  // id==kNoId -> création, sinon mise à jour
    // Upsert en lot : un seul accès disque pour tout le lot, au lieu d'un par
    // élément. Essentiel pour les imports en masse (voir ImportExportService) :
    // appeler save() en boucle relit/réécrit tout le fichier à chaque élément,
    // un coût qui explose avec le nombre de lignes (déjà observé : reset
    // watchdog après ~150s pour un import de 39 lignes avant ce correctif).
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
