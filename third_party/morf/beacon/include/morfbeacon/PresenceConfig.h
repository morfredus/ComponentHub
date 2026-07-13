/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QtGlobal>

namespace morfbeacon {

// -----------------------------------------------------------------------------
// PresenceConfig : parametres d'une application supervisee.
//
// Le heartbeat UDP annonce la PRESENCE (peu bavard, periodique). Le serveur
// HTTP local expose le DETAIL (a la demande). Les deux partagent cette config.
// -----------------------------------------------------------------------------
struct PresenceConfig {
    // Identite annoncee
    QString appName   = QStringLiteral("App");  // ex. "ComponentHub"
    QString version   = QStringLiteral("dev");  // ex. "1.4.2"
    QString instanceId;                         // optionnel ; defaut = appName@hostname

    // Heartbeat UDP (presence)
    quint16 udpPort            = 45454;  // port de broadcast (identique pour tout le parc)
    int     broadcastIntervalMs = 15000; // periode d'annonce (15 s)

    // Serveur HTTP local (detail)
    quint16 statusPort        = 8787;                    // 0 => pas de serveur HTTP
    QString statusBindAddress = QStringLiteral("0.0.0.0"); // interfaces ecoutees

    // Version du protocole, incluse dans chaque datagramme. A incrementer si le
    // format du heartbeat change de facon incompatible.
    static constexpr const char* kProto = "morfbeacon/1";
};

} // namespace morfbeacon
