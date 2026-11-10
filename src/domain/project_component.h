/**
 * ProjectComponent - Bill of materials line: an item needed by a
 * project, in a given quantity.
 *
 * Deux cas :
 *   - componentId != kNoId : refers to an inventory component.
 *   - componentId == kNoId : *off-inventory* item (not bought yet).
 *     `label` then holds its free name; it is always counted as "to buy".
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct ProjectComponent {
    Id id = kNoId;
    SyncMeta meta;        // enveloppe de synchronisation
    Id projectId = kNoId;
    Id componentId = kNoId;
    std::string label;          // used when componentId == kNoId (off-inventory item)
    int quantityRequired = 1;
};

} // namespace domain
