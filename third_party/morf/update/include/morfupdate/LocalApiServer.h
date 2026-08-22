#pragma once

#include "morfupdate/AgentConfig.h"

#include <QJsonObject>
#include <QObject>

class QTcpServer;
class QTcpSocket;

namespace morfupdate {

class OperationStore;

// Minimal HTTP boundary for the privileged agent. It is intentionally not a
// general-purpose web server: loopback only, small JSON bodies, no CORS and two
// narrowly scoped routes.
class LocalApiServer final : public QObject {
    Q_OBJECT
public:
    LocalApiServer(AgentConfig config, OperationStore* operations, QObject* parent = nullptr);
    bool start(QString* error);
    quint16 port() const;

signals:
    // The operation is stored before this signal is emitted. The caller can
    // receive a truthful 202 response while installation starts afterwards.
    void operationQueued(const QString& operationId);

private:
    void onSocketReadyRead(QTcpSocket* socket);
    void handle(QTcpSocket* socket, QByteArray method, QByteArray path,
                QByteArray headers, QByteArray body);
    void reply(QTcpSocket* socket, int code, QByteArray reason, QJsonObject body);
    static bool safeIdentifier(const QString& value);

    AgentConfig m_config;
    OperationStore* m_operations;
    QTcpServer* m_server;
};

} // namespace morfupdate
