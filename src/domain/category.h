/**
 * Category — Catégorie de composant, gérée comme une petite liste plate
 * (contrairement à Location, aucune hiérarchie n'est demandée par le cahier
 * des charges pour les catégories).
 */

#pragma once
#include <string>
#include "ids.h"

namespace domain {

struct Category {
    Id id = kNoId;
    std::string name;
};

} // namespace domain
