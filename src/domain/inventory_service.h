/**
 * InventoryService — Logique métier de l'inventaire.
 *
 * Ne connaît que les interfaces de dépôt (IComponentRepository,
 * ILocationRepository, IStockMovementRepository), jamais leur implémentation
 * (LittleFS, SQLite, fichiers plats...). C'est la classe à porter telle
 * quelle sur Raspberry Pi/Linux/Windows.
 */

#pragma once
#include <vector>
#include <optional>
#include <string>
#include "repositories.h"

namespace domain {

struct ComponentFilter {
    std::string query;      // recherche texte libre (nom, référence, fabricant, catégorie, fournisseur, notes)
    std::string category;
    std::string status;     // vide = ne filtre pas (to_test, validating, validated, defective, archived)
    Id locationId = kNoId;  // kNoId = ne filtre pas
    bool onlyLowStock = false;
    bool hasKind = false;    // false = ne filtre pas sur `kind`
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
    std::optional<Component> getComponent(Id id) const;
    // Enregistre le composant ; si son champ `category` est renseigné et ne
    // correspond à aucune Category existante, elle est créée à la volée
    // (voir _ensureCategoryExists) — la fiche composant garde un champ texte
    // libre, la liste des catégories gérées reste cependant toujours à jour.
    Component saveComponent(Component component);
    bool removeComponent(Id id);

    std::vector<Location> listLocations() const;
    Location saveLocation(Location location);
    bool removeLocation(Id id);

    std::vector<Category> listCategories() const;
    Category saveCategory(Category category);
    bool removeCategory(Id id);

    // Enregistre un mouvement de stock et met à jour la quantité du composant
    // concerné. Retourne le composant après application, ou nullopt si
    // le composant référencé n'existe pas.
    std::optional<Component> recordMovement(StockMovement movement);
    std::vector<StockMovement> history(Id componentId) const;

    // Composants sous leur seuil minimum — base de la liste d'achats automatique.
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
