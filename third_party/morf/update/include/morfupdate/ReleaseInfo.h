/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QSysInfo>
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

// Fichier annexe d'une release (somme de controle, signature, manifeste, notes) :
// jamais le binaire a installer. On le reconnait a son extension ou a son nom.
inline bool isAuxiliaryReleaseAsset(const QString& lowerName) {
    static const char* const kSuffixes[] = {
        ".sha256", ".sha512", ".sha1", ".md5", ".sig", ".asc", ".pem",
        ".txt", ".md", ".json", ".yml", ".yaml",
    };
    for (const char* s : kSuffixes) {
        if (lowerName.endsWith(QLatin1String(s)))
            return true;
    }
    // "checksums", "checksums.txt", "SHA256SUMS"... quel que soit le suffixe.
    return lowerName.contains(QLatin1String("checksum"))
        || lowerName.contains(QLatin1String("sha256sums"));
}

// Choisit, parmi les binaires d'une release GitHub, celui qui correspond au
// systeme courant (OS + architecture). Ecarte les fichiers annexes et prefere
// le format natif : .zip/.exe/.msi sous Windows, .deb (bonne architecture) puis
// .AppImage/.tar.gz sous Linux, .dmg/.pkg sous macOS. Le tri par score evite de
// tomber sur le premier asset venu (souvent checksums.sha256, en tete de liste).
// Retourne nullptr si aucun binaire exploitable : l'appelant retombe alors sur
// la page web de la release plutot que de proposer un fichier au hasard.
inline const ReleaseAsset* selectAssetForCurrentPlatform(
    const QVector<ReleaseAsset>& assets) {
#if defined(Q_OS_WIN)
    const QLatin1String os("windows");
#elif defined(Q_OS_MACOS)
    const QLatin1String os("macos");
#else
    const QLatin1String os("linux");
#endif
    // "x86_64", "arm64", "arm", "i386"... (cf. QSysInfo).
    const QString arch = QSysInfo::currentCpuArchitecture();
    const bool wantArm = arch.contains(QLatin1String("arm"))
        || arch.contains(QLatin1String("aarch"));

    const ReleaseAsset* best = nullptr;
    int bestScore = 0;
    for (const ReleaseAsset& a : assets) {
        const QString lower = a.name.toLower();
        if (lower.isEmpty() || isAuxiliaryReleaseAsset(lower))
            continue;

        int score = 0;
        // Format natif attendu pour l'OS courant.
        if (os == QLatin1String("windows")) {
            if (lower.endsWith(QLatin1String(".zip")))      score += 50;
            else if (lower.endsWith(QLatin1String(".exe"))) score += 45;
            else if (lower.endsWith(QLatin1String(".msi"))) score += 40;
            if (lower.contains(QLatin1String("win")))       score += 20;
            if (lower.contains(QLatin1String("mingw")))     score += 10;
        } else if (os == QLatin1String("macos")) {
            if (lower.endsWith(QLatin1String(".dmg")))      score += 50;
            else if (lower.endsWith(QLatin1String(".pkg"))) score += 45;
            if (lower.contains(QLatin1String("mac"))
                || lower.contains(QLatin1String("osx"))
                || lower.contains(QLatin1String("darwin"))) score += 20;
        } else { // linux
            if (lower.endsWith(QLatin1String(".deb")))            score += 50;
            else if (lower.endsWith(QLatin1String(".appimage")))  score += 45;
            else if (lower.endsWith(QLatin1String(".tar.gz"))
                     || lower.endsWith(QLatin1String(".tgz")))    score += 30;
            if (lower.contains(QLatin1String("linux")))           score += 15;
        }

        // Architecture : bonus si elle correspond, forte penalite sinon, pour ne
        // jamais proposer un arm64 a une machine x86_64 (et inversement).
        const bool assetArm = lower.contains(QLatin1String("arm64"))
            || lower.contains(QLatin1String("aarch64"));
        const bool assetX86 = lower.contains(QLatin1String("amd64"))
            || lower.contains(QLatin1String("x86_64"))
            || lower.contains(QLatin1String("x64"))
            || lower.contains(QLatin1String("win64"));
        if (wantArm) {
            if (assetArm) score += 25;
            if (assetX86) score -= 40;
        } else {
            if (assetX86) score += 25;
            if (assetArm) score -= 40;
        }

        if (score > bestScore) {
            bestScore = score;
            best = &a;
        }
    }
    // Un score nul signifie "aucun indice franc" : mieux vaut ne rien proposer
    // que de renvoyer un fichier annexe.
    return bestScore > 0 ? best : nullptr;
}

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
