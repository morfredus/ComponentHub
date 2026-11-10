/**
 * RefItem - one value of an administrable reference list.
 *
 * A reference list is a list of standardized values offered during data entry
 * (component types, manufacturers, packages, suppliers...). All
 * lists share the same mechanism: `list` identifies the reference list
 * ("component_types", "manufacturers"...), `value` is the displayed value,
 * `position` its order in the list.
 *
 * Pure value type, no platform dependency.
 */
#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct RefItem {
    Id id = kNoId;
    SyncMeta meta;         // enveloppe de synchronisation
    std::string list;      // key of the reference list
    std::string value;     // normalized value
    int position = 0;      // ordre d'affichage
};

} // namespace domain
