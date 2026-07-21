/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfbeacon/Heartbeat.h"
#include "morfbeacon/IMetricsProvider.h"

#include <QUdpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>

#include <utility>

namespace morfbeacon {

Heartbeat::Heartbeat(PresenceConfig config, IMetricsProvider* provider, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_provider(provider),
      m_socket(new QUdpSocket(this)),
      m_timer(new QTimer(this)) {
    m_timer->setInterval(m_config.broadcastIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &Heartbeat::sendNow);
}

Heartbeat::~Heartbeat() = default;

void Heartbeat::start() {
    if (m_timer->isActive())
        return;

    m_uptime.start();
    // Bind ephemere : necessaire pour emettre. ShareAddress evite un conflit si
    // plusieurs applications morfBeacon tournent sur la meme machine.
    m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), 0,
                   QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

    sendNow();          // annonce immediate au demarrage
    m_timer->start();
}

void Heartbeat::stop() {
    m_timer->stop();
    m_socket->close();
}

bool Heartbeat::isRunning() const {
    return m_timer->isActive();
}

void Heartbeat::setStatusPort(quint16 port) {
    m_config.statusPort = port;
}

void Heartbeat::sendNow() {
    const QByteArray dgram = buildDatagram();

    // Emission sur l'adresse de broadcast de CHAQUE interface active (plus
    // fiable que le broadcast global sur une machine multi-reseaux : WiFi +
    // Ethernet + VM). Repli sur 255.255.255.255 si aucune interface exploitable.
    bool sent = false;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull())
                continue;
            m_socket->writeDatagram(dgram, bcast, m_config.udpPort);
            sent = true;
        }
    }

    if (!sent)
        m_socket->writeDatagram(dgram, QHostAddress(QHostAddress::Broadcast),
                                m_config.udpPort);
}

QByteArray Heartbeat::buildDatagram() const {
    const QString host = QHostInfo::localHostName();

    QJsonObject o;
    o["proto"]       = QString::fromLatin1(PresenceConfig::kProto);
    o["app"]         = m_config.appName;
    o["host"]        = host;
    o["version"]     = m_config.version;
    o["state"]       = m_provider ? m_provider->state() : QStringLiteral("ok");
    o["status_port"] = static_cast<int>(m_config.statusPort);
    o["instance"]    = m_config.instanceId.isEmpty()
                           ? (m_config.appName + QStringLiteral("@") + host)
                           : m_config.instanceId;
    // Capacites : emises seulement si declarees, pour que le datagramme reste
    // court quand un service n'en annonce aucune.
    //
    // « web_ui » est derivee de webUiPath plutot que declaree separement : le
    // detail de l'interface et la capacite qui la rend decouvrable ne peuvent
    // donc pas diverger. Declarer l'un sans l'autre produirait soit une
    // interface introuvable, soit un lien vers rien.
    QStringList caps = m_config.capabilities;
    if (!m_config.webUiPath.isEmpty() &&
        !caps.contains(QLatin1String(PresenceConfig::kCapabilityWebUi)))
        caps << QLatin1String(PresenceConfig::kCapabilityWebUi);

    if (!caps.isEmpty()) {
        QJsonArray arr;
        for (const QString& c : caps)
            arr.append(c);
        o["capabilities"] = arr;
    }
    o["uptime_s"]    = static_cast<double>(m_uptime.isValid() ? m_uptime.elapsed() / 1000 : 0);
    o["ts"]          = static_cast<double>(QDateTime::currentSecsSinceEpoch());

    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

} // namespace morfbeacon
