/**
 * IReferentialRepository — accès aux valeurs des référentiels.
 *
 * Une seule interface pour TOUTES les listes (mécanisme générique) : le
 * paramètre `list` sélectionne le référentiel concerné.
 */
#pragma once
#include <vector>
#include "referential.h"

namespace domain {

class IReferentialRepository {
public:
    virtual ~IReferentialRepository() = default;
    virtual std::vector<RefItem> findAll() const = 0;
    // Valeurs d'un référentiel, triées par `position` puis alphabétiquement.
    virtual std::vector<RefItem> findByList(const std::string& list) const = 0;
    virtual RefItem save(RefItem item) = 0;
    virtual std::vector<RefItem> saveAll(const std::vector<RefItem>& items) = 0;  // upsert en lot (réordonnancement)
    virtual bool remove(Id id) = 0;
};

} // namespace domain
