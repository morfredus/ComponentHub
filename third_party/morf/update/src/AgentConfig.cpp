#include "morfupdate/AgentConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace morfupdate {

bool AgentConfig::load(const QString& path, AgentConfig* config, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("agent configuration is not a JSON object");
        return false;
    }
    const QJsonObject root = doc.object();
    const int port = root.value(QStringLiteral("http_port")).toInt(8794);
    const QString bind = root.value(QStringLiteral("bind_address")).toString(QStringLiteral("127.0.0.1"));
    if (port < 1 || port > 65535 || bind != QStringLiteral("127.0.0.1")) {
        if (error) *error = QStringLiteral("agent must use loopback");
        return false;
    }
    AgentConfig parsed;
    parsed.httpPort = static_cast<quint16>(port);
    parsed.bindAddress = bind;
    for (const QJsonValue& value : root.value(QStringLiteral("targets")).toArray()) {
        const QJsonObject obj = value.toObject();
        AgentTarget target{obj.value(QStringLiteral("project")).toString(),
                           obj.value(QStringLiteral("service")).toString(),
                           obj.value(QStringLiteral("repository")).toString(),
                           obj.value(QStringLiteral("health_url")).toString(),
                           obj.value(QStringLiteral("app_dir")).toString(),
                           obj.value(QStringLiteral("service_manager")).toString()};
        if (target.project.isEmpty() || target.service.isEmpty() || target.repository.isEmpty()
            || target.healthUrl.isEmpty() || parsed.targets.contains(target.project)) {
            if (error) *error = QStringLiteral("agent target declarations are invalid");
            return false;
        }
#ifdef Q_OS_WIN
        if (target.serviceManager != QStringLiteral("scm")
            && target.serviceManager != QStringLiteral("task")) {
            if (error) *error = QStringLiteral("Windows target must declare service_manager scm or task");
            return false;
        }
#endif
        parsed.targets.insert(target.project, target);
    }
    *config = std::move(parsed);
    return true;
}

} // namespace morfupdate
