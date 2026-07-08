/**
 * Project — Fiche projet (cf. cahier des charges : nom, description,
 * version, firmware, dépôt Git, documentation, composants utilisés).
 */

#pragma once
#include <string>
#include "ids.h"

namespace domain {

struct Project {
    Id id = kNoId;
    std::string name;
    std::string description;
    std::string version;
    std::string firmware;
    std::string gitRepo;
    std::string status;  // texte libre : en cours, terminé, archivé...
    std::string notes;
};

} // namespace domain
