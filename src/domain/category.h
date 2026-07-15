/**
 * Category — Catégorie de composant, gérée comme une petite liste plate
 * (contrairement à Location, aucune hiérarchie n'est demandée par le cahier
 * des charges pour les catégories).
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct Category {
    Id id = kNoId;
    SyncMeta meta;        // enveloppe de synchronisation
    std::string name;
};

} // namespace domain
