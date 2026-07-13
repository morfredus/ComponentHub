/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include "morfupdate/ReleaseInfo.h"

namespace morfupdate {

// -----------------------------------------------------------------------------
// IUpdateSource : source enfichable de la derniere version disponible.
//
// L'implementation par defaut est GitHubReleaseSource, mais on peut brancher un
// manifeste JSON auto-heberge (o2switch), un stub de test, etc. La recuperation
// est asynchrone : le resultat arrive via un signal, jamais en bloquant l'UI.
// -----------------------------------------------------------------------------
class IUpdateSource : public QObject {
    Q_OBJECT
public:
    explicit IUpdateSource(QObject* parent = nullptr) : QObject(parent) {}
    ~IUpdateSource() override = default;

    // Lance la recuperation. Emet 'latestReady' ou 'failed'.
    virtual void fetchLatest(bool includePrereleases) = 0;

signals:
    void latestReady(const morfupdate::ReleaseInfo& info);
    void failed(const QString& error);
};

} // namespace morfupdate
