/**
 * Location - Physical location, arbitrarily hierarchical via
 * parentId (e.g. Workshop > Cabinet A > Drawer A12 > ESD box > Bag 3).
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct Location {
    Id id = kNoId;
    SyncMeta meta;        // enveloppe de synchronisation
    std::string name;
    Id parentId = kNoId;  // kNoId = emplacement racine
};

} // namespace domain
