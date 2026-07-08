/**
 * Location — Emplacement physique, arbitrairement hiérarchisable via
 * parentId (ex: Atelier > Armoire A > Tiroir A12 > Boîte ESD > Sachet 3).
 */

#pragma once
#include <string>
#include "ids.h"

namespace domain {

struct Location {
    Id id = kNoId;
    std::string name;
    Id parentId = kNoId;  // kNoId = emplacement racine
};

} // namespace domain
