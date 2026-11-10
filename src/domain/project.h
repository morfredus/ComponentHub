/**
 * Project - Project record (see the specification: name, description,
 * version, firmware, Git repository, documentation, components used).
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct Project {
    Id id = kNoId;
    SyncMeta meta;        // enveloppe de synchronisation
    std::string name;
    std::string description;
    std::string version;
    std::string firmware;
    std::string gitRepo;
    std::string status;  // free text: in progress, done, archived...
    std::string notes;
};

} // namespace domain
