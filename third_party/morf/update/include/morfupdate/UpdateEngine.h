/*
 * morfUpdate - verified update executor
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "morfupdate/AgentConfig.h"

#include <QObject>

namespace morfupdate {

class OperationStore;

// Executes an already accepted operation.  The public API can never provide a
// URL, command or filesystem path: this class derives every one of them from
// the installed configuration and the release manifest.
class UpdateEngine final : public QObject {
    Q_OBJECT
public:
    UpdateEngine(AgentConfig config, OperationStore* operations, QString stateDirectory,
                 QObject* parent = nullptr);

public slots:
    void run(const QString& operationId);

    // Relance manuelle d'un service déjà installé : ni téléchargement, ni
    // vérification, ni installation. Réutilise le journal d'opérations et le
    // helper privilégié (verbe --restart), puis vérifie /healthz. Séparé de run()
    // pour qu'un restart ne parte jamais dans le chemin d'installation.
    void restart(const QString& operationId);

private:
    bool fail(const QString& operationId, const QString& detail);
    AgentConfig m_config;
    OperationStore* m_operations;
    QString m_stateDirectory;
};

} // namespace morfupdate
