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
                           obj.value(QStringLiteral("service_manager")).toString(),
                           obj.value(QStringLiteral("self")).toBool(false)};
        if (target.project.isEmpty() || target.service.isEmpty() || target.repository.isEmpty()
            || target.healthUrl.isEmpty() || parsed.targets.contains(target.project)) {
            if (error) *error = QStringLiteral("agent target declarations are invalid");
            return false;
        }
        // La PLATEFORME cible fait foi pour le « comment » (gestionnaire de service
        // + dossier d'installation) ; la config ne declare que le « quoi » (projet,
        // service, depot, health), commun a toutes les plateformes. On DERIVE donc
        // ces deux champs de l'OS COURANT au lieu de faire confiance a la config.
        // Sans cela, un morfupdate.json ecrit pour Windows (service_manager=task,
        // app_dir=%ProgramData%/...) deploye tel quel sur un Pi Linux faisait croire
        // a l'agent qu'il gerait une tache Windows dans %ProgramData% -- l'update
        // echouait alors sur un ARM64 pourtant sain. La meme config marche desormais
        // sur Windows, Linux x64 et ARM64.
#ifdef Q_OS_WIN
        // Windows offre deux gestionnaires (scm/task) : la config choisit, defaut task.
        if (target.serviceManager != QStringLiteral("scm")
            && target.serviceManager != QStringLiteral("task"))
            target.serviceManager = QStringLiteral("task");
        if (target.appDir.isEmpty())
            target.appDir = QStringLiteral("%ProgramData%/") + target.service;
#else
        // Linux : toujours systemd, toujours /opt/<service> (convention morfdeploy).
        // On ecrase ce que dit la config : la machine cible est la source de verite.
        target.serviceManager = QStringLiteral("systemd");
        target.appDir = QStringLiteral("/opt/") + target.service;
#endif
        parsed.targets.insert(target.project, target);
    }
    *config = std::move(parsed);
    return true;
}

} // namespace morfupdate
