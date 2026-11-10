/**
 * Category - Component category, managed as a small flat list
 * (unlike Location, the specification asks for no hierarchy
 * for categories).
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
