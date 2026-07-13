/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include "morfbeacon/PresenceConfig.h"

namespace morfbeacon {

class IMetricsProvider;
class Heartbeat;
class StatusServer;

// -----------------------------------------------------------------------------
// PresenceService : facade qui cable le Heartbeat (presence UDP) et le
// StatusServer (detail HTTP) a partir d'une seule config et d'un fournisseur
// de metriques. C'est le seul objet que l'application a besoin de manipuler.
//
// Integration type (quelques lignes) :
//
//   morfbeacon::PresenceConfig cfg;
//   cfg.appName = "ComponentHub";
//   cfg.version = APP_VERSION;
//   morfbeacon::PresenceService presence(cfg, &monProvider);
//   presence.start();
// -----------------------------------------------------------------------------
class PresenceService : public QObject {
    Q_OBJECT
public:
    explicit PresenceService(PresenceConfig config,
                             IMetricsProvider* provider = nullptr,
                             QObject* parent = nullptr);
    ~PresenceService() override;

    // Demarre le serveur HTTP puis le heartbeat. Renvoie true si le serveur
    // HTTP est operationnel (toujours true si statusPort == 0). Le heartbeat
    // demarre dans tous les cas.
    bool start();
    void stop();

    Heartbeat*    heartbeat() const;
    StatusServer* statusServer() const;

private:
    PresenceConfig m_config;
    Heartbeat*     m_heartbeat;
    StatusServer*  m_status;
};

} // namespace morfbeacon
