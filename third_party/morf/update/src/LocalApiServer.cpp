#include "morfupdate/LocalApiServer.h"
#include "morfupdate/OperationStore.h"
#include "morfupdate/UpdateOperation.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

namespace morfupdate {
namespace {
constexpr int kMaxRequestBytes = 16 * 1024;

int contentLength(const QByteArray& headers) {
    for (const QByteArray& line : headers.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:"))
            return trimmed.mid(trimmed.indexOf(':') + 1).trimmed().toInt();
    }
    return 0;
}

QString platformName() {
#ifdef Q_OS_WIN
    return QStringLiteral("windows-x86_64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("linux-arm64");
#else
    return QStringLiteral("linux-amd64");
#endif
}
}

LocalApiServer::LocalApiServer(AgentConfig config, OperationStore* operations, QObject* parent)
    : QObject(parent), m_config(std::move(config)), m_operations(operations),
      m_server(new QTcpServer(this)) {
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (m_server->hasPendingConnections()) {
            QTcpSocket* socket = m_server->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this,
                    [this, socket]() { onSocketReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

bool LocalApiServer::start(QString* error) {
    if (!m_server->listen(QHostAddress::LocalHost, m_config.httpPort)) {
        if (error) *error = m_server->errorString();
        return false;
    }
    return true;
}

quint16 LocalApiServer::port() const { return m_server->serverPort(); }

void LocalApiServer::onSocketReadyRead(QTcpSocket* socket) {
    QByteArray buffer = socket->property("morfupdate-buffer").toByteArray();
    buffer += socket->readAll();
    if (buffer.size() > kMaxRequestBytes) { socket->disconnectFromHost(); return; }
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) { socket->setProperty("morfupdate-buffer", buffer); return; }
    const QByteArray headers = buffer.left(headerEnd);
    const int length = contentLength(headers);
    if (length < 0 || length > kMaxRequestBytes || buffer.size() < headerEnd + 4 + length) {
        socket->setProperty("morfupdate-buffer", buffer); return;
    }
    const QList<QByteArray> request = headers.left(headers.indexOf("\r\n")).split(' ');
    handle(socket, request.value(0), request.value(1), headers,
           buffer.mid(headerEnd + 4, length));
}

void LocalApiServer::handle(QTcpSocket* socket, QByteArray method, QByteArray path,
                            QByteArray headers, QByteArray body) {
    // Liveness and the capability scope carry no administrative information.
    // They remain intentionally readable so a local supervisor can tell an
    // unavailable agent from an agent that refuses an update request.
    if (method == "GET" && path == "/healthz") {
        reply(socket, 200, "OK", {{"status", "ok"}});
        return;
    }
    if (method == "GET" && path == "/status") {
        reply(socket, 200, "OK", {{"app", "morfUpdate"}, {"state", "ok"},
              {"updates", QJsonObject{{"scope", "local"}}}});
        return;
    }
    if (method == "GET" && path.startsWith("/api/v1/updates/")) {
        const QString id = QString::fromUtf8(path.mid(QByteArray("/api/v1/updates/").size()));
        const UpdateOperation* operation = m_operations->find(id);
        if (!operation) { reply(socket, 404, "Not Found", {{"error", "operation not found"}}); return; }
        reply(socket, 200, "OK", {{"id", operation->id}, {"project", operation->project},
              {"from_version", operation->fromVersion}, {"to_version", operation->toVersion},
              {"platform", operation->platform}, {"state", updateStateName(operation->state)},
              {"detail", operation->detail}, {"created_at", operation->createdAt.toString(Qt::ISODate)},
              {"updated_at", operation->updatedAt.toString(Qt::ISODate)}});
        return;
    }
    if (method != "POST" || path != "/api/v1/updates") {
        reply(socket, 404, "Not Found", {{"error", "route not found"}}); return;
    }
    const QJsonDocument request = QJsonDocument::fromJson(body);
    const QJsonObject object = request.object();
    const QString project = object.value("project").toString();
    const QString version = object.value("version").toString();
    if (!request.isObject() || !safeIdentifier(project) || !safeIdentifier(version)
        || !m_config.targets.contains(project)) {
        reply(socket, 400, "Bad Request", {{"error", "project and version must be declared identifiers"}});
        return;
    }
    if (project == QStringLiteral("morfUpdate")) {
        reply(socket, 409, "Conflict", {{"error", "morfUpdate cannot update itself"}}); return;
    }
    if (const UpdateOperation* active = m_operations->active()) {
        reply(socket, 409, "Conflict", {{"error", "another update is active"}, {"id", active->id},
              {"state", updateStateName(active->state)}}); return;
    }
    QString error;
    const UpdateOperation operation = m_operations->create(project, QString(), version, platformName(), &error);
    if (operation.id.isEmpty()) { reply(socket, 500, "Internal Server Error", {{"error", error}}); return; }
    reply(socket, 202, "Accepted", {{"id", operation.id}, {"state", "queued"}});
    emit operationQueued(operation.id);
}

void LocalApiServer::reply(QTcpSocket* socket, int code, QByteArray reason, QJsonObject body) {
    const QByteArray content = QJsonDocument(body).toJson(QJsonDocument::Compact);
    socket->write("HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\n"
                  "Content-Type: application/json\r\nCache-Control: no-store\r\n"
                  "Content-Length: " + QByteArray::number(content.size()) + "\r\nConnection: close\r\n\r\n" + content);
    socket->disconnectFromHost();
}

bool LocalApiServer::safeIdentifier(const QString& value) {
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"));
    return allowed.match(value).hasMatch();
}

} // namespace morfupdate
