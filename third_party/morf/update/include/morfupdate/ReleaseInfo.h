/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QUrl>
#include <QVector>
#include "morfupdate/Version.h"

namespace morfupdate {

// Un binaire telechargeable attache a une release (ex. le .zip Windows).
struct ReleaseAsset {
    QString name;
    QUrl    url;        // lien de telechargement direct
    qint64  size = 0;   // octets
};

// -----------------------------------------------------------------------------
// ReleaseInfo : description d'une version publiee, independante de la source
// (GitHub, manifeste perso...). C'est ce que UpdateChecker compare a la version
// courante.
// -----------------------------------------------------------------------------
struct ReleaseInfo {
    Version version;              // extraite du tag
    QString tag;                  // ex. "v1.5.0"
    QString name;                 // titre de la release
    QString notes;                // corps / changelog (Markdown)
    QUrl    htmlUrl;              // page web de la release
    QString publishedAt;          // date ISO 8601
    bool    prerelease = false;
    QVector<ReleaseAsset> assets;
    bool    valid = false;
};

} // namespace morfupdate
