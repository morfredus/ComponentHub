/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/Version.h"

#include <QStringList>

namespace morfupdate {

Version Version::parse(const QString& text) {
    Version v;
    QString s = text.trimmed();
    if (s.isEmpty())
        return v;

    // Prefixe "v" / "V" tolere (ex. tags "v1.4.2").
    if (s.startsWith('v') || s.startsWith('V'))
        s = s.mid(1);

    // Separation d'un eventuel suffixe de pre-release / build.
    const int dash = s.indexOf('-');
    QString core = dash >= 0 ? s.left(dash) : s;
    if (dash >= 0)
        v.pre = s.mid(dash + 1);

    const QStringList parts = core.split('.');
    auto toInt = [](const QString& p, bool& ok) {
        return p.toInt(&ok);
    };

    bool ok = true;
    bool anyOk = false;
    if (parts.size() >= 1) { v.major = toInt(parts[0], ok); anyOk = anyOk || ok; }
    if (parts.size() >= 2) { v.minor = toInt(parts[1], ok); }
    if (parts.size() >= 3) { v.patch = toInt(parts[2], ok); }

    // Valide si au moins le champ majeur est un entier exploitable.
    v.valid = anyOk;
    return v;
}

int Version::compare(const Version& other) const {
    if (major != other.major) return major < other.major ? -1 : 1;
    if (minor != other.minor) return minor < other.minor ? -1 : 1;
    if (patch != other.patch) return patch < other.patch ? -1 : 1;

    // Meme coeur : une stable (pre vide) l'emporte sur une pre-release.
    const bool aPre = !pre.isEmpty();
    const bool bPre = !other.pre.isEmpty();
    if (aPre != bPre) return aPre ? -1 : 1;   // *this pre-release => anterieure
    if (!aPre && !bPre) return 0;             // deux stables identiques

    // Deux pre-releases : comparaison lexicale simple, suffisante ici.
    return QString::compare(pre, other.pre);
}

QString Version::toString() const {
    QString s = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
    if (!pre.isEmpty())
        s += '-' + pre;
    return s;
}

} // namespace morfupdate
