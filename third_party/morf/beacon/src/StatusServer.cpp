/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfbeacon/StatusServer.h"
#include "morfbeacon/IMetricsProvider.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>

#include <utility>

namespace morfbeacon {

namespace {
constexpr int kMaxRequestBytes = 8192; // garde-fou : une requete GET est minuscule
}

StatusServer::StatusServer(PresenceConfig config, IMetricsProvider* provider, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_provider(provider),
      m_server(new QTcpServer(this)) {
    connect(m_server, &QTcpServer::newConnection, this, &StatusServer::onNewConnection);
}

StatusServer::~StatusServer() = default;

bool StatusServer::start() {
    if (m_config.statusPort == 0)
        return false;

    m_uptime.start();

    QHostAddress addr(m_config.statusBindAddress);
    if (addr.isNull())
        addr = QHostAddress(QHostAddress::AnyIPv4);

    return m_server->listen(addr, m_config.statusPort);
}

void StatusServer::stop() {
    m_server->close();
}

bool StatusServer::isListening() const {
    return m_server->isListening();
}

quint16 StatusServer::port() const {
    return m_server->isListening() ? m_server->serverPort() : 0;
}

void StatusServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();

        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            QByteArray buf = sock->property("buf").toByteArray();
            buf += sock->readAll();

            const int headerEnd = buf.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                if (buf.size() > kMaxRequestBytes) {
                    sock->abort();
                    return;
                }
                sock->setProperty("buf", buf); // en-tetes incomplets : on attend la suite
                return;
            }

            const int lineEnd = buf.indexOf("\r\n");
            const QByteArray requestLine = buf.left(lineEnd);
            handleRequest(sock, requestLine);
        });

        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void StatusServer::handleRequest(QTcpSocket* sock, const QByteArray& requestLine) {
    const QList<QByteArray> parts = requestLine.split(' ');
    const QByteArray method = parts.value(0);
    const QByteArray path   = parts.value(1);

    int        code   = 200;
    QByteArray reason  = "OK";
    QByteArray body;

    if (method != "GET") {
        code = 405; reason = "Method Not Allowed";
        body = "{\"error\":\"method not allowed\"}";
    } else if (path == "/status" || path.startsWith("/status?")) {
        body = buildStatusJson();
    } else if (path == "/healthz") {
        body = "{\"status\":\"ok\"}";
    } else {
        code = 404; reason = "Not Found";
        body = "{\"error\":\"not found\"}";
    }

    QByteArray resp;
    resp += "HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\n";
    resp += "Content-Type: application/json; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n"; // autorise un futur dashboard web
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;

    sock->write(resp);
    sock->flush();
    sock->disconnectFromHost();
}

QByteArray StatusServer::buildStatusJson() const {
    QJsonObject o;
    o["app"]      = m_config.appName;
    o["host"]     = QHostInfo::localHostName();
    o["version"]  = m_config.version;
    o["state"]    = m_provider ? m_provider->state() : QStringLiteral("ok");
    o["uptime_s"] = static_cast<double>(m_uptime.isValid() ? m_uptime.elapsed() / 1000 : 0);
    o["ts"]       = static_cast<double>(QDateTime::currentSecsSinceEpoch());
    o["metrics"]  = m_provider ? m_provider->metrics() : QJsonObject{};

    // Detail de l'interface Web : publie ICI et pas dans le heartbeat. Le
    // datagramme annonce la CAPACITE (« web_ui »), ce document en donne les
    // moyens d'ouverture. Un consommateur n'interroge donc /status que pour les
    // services qui ont declare la capacite, et le trafic periodique reste
    // inchange.
    //
    // Absent si aucune interface n'est declaree : un consommateur ne doit pas
    // avoir a distinguer « pas d'interface » de « interface vide ».
    if (!m_config.webUiPath.isEmpty()) {
        QJsonObject ui;
        ui["path"]  = m_config.webUiPath;
        ui["label"] = m_config.webUiLabel.isEmpty() ? m_config.appName
                                                    : m_config.webUiLabel;
        // Port explicite : le consommateur n'a pas a deviner que l'interface
        // partage le port de /status, meme si c'est le cas le plus courant.
        ui["port"]  = static_cast<int>(m_config.webUiPort != 0 ? m_config.webUiPort
                                                               : m_config.statusPort);
        if (!m_config.webUiDescription.isEmpty())
            ui["description"] = m_config.webUiDescription;
        o["web_ui"] = ui;
    }

    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

} // namespace morfbeacon
