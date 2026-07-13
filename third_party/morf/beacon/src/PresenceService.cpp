/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfbeacon/PresenceService.h"
#include "morfbeacon/Heartbeat.h"
#include "morfbeacon/StatusServer.h"

#include <utility>

namespace morfbeacon {

PresenceService::PresenceService(PresenceConfig config, IMetricsProvider* provider, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_heartbeat(new Heartbeat(m_config, provider, this)),
      m_status(new StatusServer(m_config, provider, this)) {}

PresenceService::~PresenceService() = default;

bool PresenceService::start() {
    quint16 advertisedPort = 0;
    bool httpOk = true;

    if (m_config.statusPort != 0) {
        httpOk = m_status->start();
        // On annonce le port HTTP seulement s'il est reellement joignable, pour
        // ne pas envoyer le superviseur vers un port ferme.
        advertisedPort = httpOk ? m_status->port() : 0;
    }

    m_heartbeat->setStatusPort(advertisedPort);
    m_heartbeat->start();
    return httpOk;
}

void PresenceService::stop() {
    m_heartbeat->stop();
    m_status->stop();
}

Heartbeat*    PresenceService::heartbeat() const    { return m_heartbeat; }
StatusServer* PresenceService::statusServer() const { return m_status; }

} // namespace morfbeacon
