/**
 * RefItem — une valeur d'un référentiel administrable.
 *
 * Un référentiel est une liste de valeurs normalisées proposée à la saisie
 * (types de composants, fabricants, boîtiers, fournisseurs...). Toutes les
 * listes partagent le même mécanisme : `list` identifie le référentiel
 * (« component_types », « manufacturers »...), `value` est la valeur affichée,
 * `position` son ordre dans la liste.
 *
 * Type de valeur pur, sans dépendance plateforme.
 */
#pragma once
#include <string>
#include "ids.h"

namespace domain {

struct RefItem {
    Id id = kNoId;
    std::string list;      // clé du référentiel
    std::string value;     // valeur normalisée
    int position = 0;      // ordre d'affichage
};

} // namespace domain
