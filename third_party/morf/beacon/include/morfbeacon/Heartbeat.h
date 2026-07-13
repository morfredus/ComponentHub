/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include <QElapsedTimer>
#include "morfbeacon/PresenceConfig.h"

class QUdpSocket;
class QTimer;

namespace morfbeacon {

class IMetricsProvider;

// -----------------------------------------------------------------------------
// Heartbeat : emet periodiquement un petit datagramme UDP en broadcast pour
// annoncer la presence de l'application sur le reseau local.
//
// Le datagramme (JSON compact) contient uniquement l'essentiel :
//   proto, app, host, version, state, status_port, instance, uptime_s, ts
//
// Il ne contient AUCUNE metrique detaillee : celles-ci vivent derriere le
// serveur HTTP (StatusServer), interroge a la demande.
// -----------------------------------------------------------------------------
class Heartbeat : public QObject {
    Q_OBJECT
public:
    explicit Heartbeat(PresenceConfig config,
                       IMetricsProvider* provider = nullptr,
                       QObject* parent = nullptr);
    ~Heartbeat() override;

    void start();
    void stop();
    bool isRunning() const;

    // Emet immediatement une annonce (aussi appelee par le timer).
    void sendNow();

    // Met a jour le port HTTP annonce (utilise par PresenceService quand le
    // port reel differe du port configure).
    void setStatusPort(quint16 port);

private:
    QByteArray buildDatagram() const;

    PresenceConfig    m_config;
    IMetricsProvider* m_provider;
    QUdpSocket*       m_socket;
    QTimer*           m_timer;
    QElapsedTimer     m_uptime;
};

} // namespace morfbeacon
