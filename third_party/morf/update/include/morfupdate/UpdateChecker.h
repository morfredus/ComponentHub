/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include "morfupdate/morfUpdateConfig.h"
#include "morfupdate/ReleaseInfo.h"

namespace morfupdate {

class IUpdateSource;

// -----------------------------------------------------------------------------
// UpdateChecker : compare la version courante a la derniere version publiee et
// signale le resultat. Asynchrone et non bloquant.
//
//   - updateAvailable : une version plus recente existe
//   - upToDate        : deja a jour (fournit la derniere connue)
//   - checkFailed     : reseau / parsing / depot introuvable
// -----------------------------------------------------------------------------
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    // Utilise GitHub Releases par defaut.
    explicit UpdateChecker(morfUpdateConfig config, QObject* parent = nullptr);

    // Source enfichable (tests, manifeste perso). Prend 'source' pour parent.
    UpdateChecker(morfUpdateConfig config, IUpdateSource* source, QObject* parent = nullptr);

    ~UpdateChecker() override;

    void checkForUpdates();
    QString currentVersion() const;

signals:
    void updateAvailable(const morfupdate::ReleaseInfo& info);
    void upToDate(const morfupdate::ReleaseInfo& latest);
    void checkFailed(const QString& error);

private:
    void wire();

    morfUpdateConfig m_config;
    IUpdateSource*      m_source;
};

} // namespace morfupdate
