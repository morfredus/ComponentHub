/**
 * Location — Emplacement physique, arbitrairement hiérarchisable via
 * parentId (ex: Atelier > Armoire A > Tiroir A12 > Boîte ESD > Sachet 3).
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
