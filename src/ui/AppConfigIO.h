/**
 * AppConfigIO.h - Sauvegarde / restauration de la configuration applicative.
 *
 * La configuration (adresse du hub, jeton, options de synchro, apparence…) vit
 * dans QSettings (registre Windows / fichier INI), donc HORS du dossier de
 * données. Ces helpers la sérialisent en JSON pour :
 *   - l'inclure dans la sauvegarde complète (app_config.json, SANS le jeton, ajouté
 *     directement à l'archive : aucun fichier de config ne reste dans le dossier de données) ;
 *   - offrir une sauvegarde/restauration de la configuration SEULE.
 */

#pragma once
#include <QString>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QByteArray>

namespace chui {

// Nom fixe du fichier de config déposé dans le dossier de données.
inline QString configPathInDir(const QString& dataDir) { return dataDir + "/app_config.json"; }

// Sérialise toutes les clés de configuration en JSON. Valeurs stockées en texte : la
// relecture via QVariant::toBool()/toInt()/toString() reste correcte. Le jeton de synchro
// (sync/token) n'est inclus que sur demande : une sauvegarde archivée puis déposée sur un
// partage ne doit pas emporter un secret. Rappel : ce jeton circule en HTTP clair vers le hub,
// à réserver à un LAN de confiance.
inline QByteArray configJson(bool includeToken) {
    QSettings s("morfredus", "ComponentHub");
    QJsonObject obj;
    for (const QString& k : s.allKeys()) {
        if (!includeToken && k == "sync/token") continue;
        obj[k] = s.value(k).toString();
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

// Écrit la configuration dans un fichier JSON.
inline bool exportConfig(const QString& path, bool includeToken = true) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(configJson(includeToken));
    return true;
}

// Recharge la configuration depuis un fichier JSON dans QSettings.
inline bool importConfig(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    QSettings s("morfredus", "ComponentHub");
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) s.setValue(it.key(), it.value().toString());
    s.sync();
    return true;
}

} // namespace chui
