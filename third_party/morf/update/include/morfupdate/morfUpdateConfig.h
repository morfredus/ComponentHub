/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>

namespace morfupdate {

// -----------------------------------------------------------------------------
// morfUpdateConfig : parametres d'une verification de mise a jour.
// -----------------------------------------------------------------------------
struct morfUpdateConfig {
    QString owner;                    // proprietaire GitHub, ex. "morfredus"
    QString repo;                     // depot, ex. "SiteWatch"
    QString currentVersion;           // version installee, ex. SITEWATCH_VERSION
    bool    includePrereleases = false; // proposer aussi les beta ?
    QString githubToken;              // optionnel : depots prives / quota API
};

} // namespace morfupdate
