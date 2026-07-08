/**
 * ProjectComponent — Ligne de nomenclature : un élément nécessaire à un
 * projet, en une certaine quantité.
 *
 * Deux cas :
 *   - componentId != kNoId : renvoie à un composant de l'inventaire.
 *   - componentId == kNoId : élément *hors inventaire* (pas encore acheté).
 *     `label` porte alors son nom libre ; il est toujours compté « à acheter ».
 */

#pragma once
#include <string>
#include "ids.h"

namespace domain {

struct ProjectComponent {
    Id id = kNoId;
    Id projectId = kNoId;
    Id componentId = kNoId;
    std::string label;          // utilisé quand componentId == kNoId (élément hors inventaire)
    int quantityRequired = 1;
};

} // namespace domain
