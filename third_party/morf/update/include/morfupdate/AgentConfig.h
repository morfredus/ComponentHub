#pragma once

#include <QHash>
#include <QString>

namespace morfupdate {

// Configuration deliberately contains only stable identities. The HTTP client
// never supplies a repository, URL, service unit or filesystem path.
struct AgentTarget {
    QString project;
    QString service;
    QString repository;
    QString healthUrl;
    QString appDir;
    QString serviceManager;
    // Cible = morfUpdate lui-meme. Opt-in explicite ("self": true dans la config) :
    // sans cela l'agent refuse toujours de se mettre a jour (comportement historique).
    // Active le chemin de succession en deux temps (applieur systemd detache).
    bool    isSelf = false;
};

struct AgentConfig {
    quint16 httpPort = 8794;
    QString bindAddress = QStringLiteral("127.0.0.1");
    QHash<QString, AgentTarget> targets;

    static bool load(const QString& path, AgentConfig* config, QString* error);
};

} // namespace morfupdate
