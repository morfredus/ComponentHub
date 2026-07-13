/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>

namespace morfupdate {

// -----------------------------------------------------------------------------
// Version : version semantique simplifiee (major.minor.patch[-prerelease]).
//
// Tolere un prefixe "v" (ex. "v1.4.2") et un suffixe de pre-release
// (ex. "1.5.0-beta.1"). Une pre-release est consideree ANTERIEURE a la release
// stable de meme numero (1.5.0-beta < 1.5.0), conformement a SemVer.
// -----------------------------------------------------------------------------
struct Version {
    int     major = 0;
    int     minor = 0;
    int     patch = 0;
    QString pre;            // suffixe de pre-release, vide si stable
    bool    valid = false;

    static Version parse(const QString& text);

    // <0 si *this precede other, 0 si egales, >0 si *this suit other.
    int compare(const Version& other) const;

    bool operator<(const Version& o)  const { return compare(o) <  0; }
    bool operator>(const Version& o)  const { return compare(o) >  0; }
    bool operator==(const Version& o) const { return compare(o) == 0; }

    QString toString() const;
};

} // namespace morfupdate
