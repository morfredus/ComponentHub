/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QObject>
#include <QElapsedTimer>
#include "morfbeacon/PresenceConfig.h"

class QTcpServer;
class QTcpSocket;

namespace morfbeacon {

class IMetricsProvider;

// -----------------------------------------------------------------------------
// StatusServer : minuscule serveur HTTP/1.1 local exposant le DETAIL de l'app.
//
//   GET /status   -> JSON complet { app, host, version, state, uptime_s,
//                                    metrics: {...}, ts }
//   GET /healthz  -> { "status": "ok" }  (sonde de vie legere)
//
// Interroge a la demande par le superviseur (RaspberryDashboard), jamais en
// continu. Volontairement minimal : une requete, une reponse, connexion fermee.
// -----------------------------------------------------------------------------
class StatusServer : public QObject {
    Q_OBJECT
public:
    explicit StatusServer(PresenceConfig config,
                          IMetricsProvider* provider = nullptr,
                          QObject* parent = nullptr);
    ~StatusServer() override;

    bool start();            // false si le port ne peut etre ouvert
    void stop();
    bool isListening() const;
    quint16 port() const;    // port reellement ecoute (0 si arrete)

private:
    void onNewConnection();
    void handleRequest(QTcpSocket* sock, const QByteArray& requestLine);
    QByteArray buildStatusJson() const;

    PresenceConfig    m_config;
    IMetricsProvider* m_provider;
    QTcpServer*       m_server;
    QElapsedTimer     m_uptime;
};

} // namespace morfbeacon
