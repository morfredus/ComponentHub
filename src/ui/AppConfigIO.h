/**
 * AppConfigIO.h — Sauvegarde / restauration de la configuration applicative.
 *
 * La configuration (adresse du hub, jeton, options de synchro, apparence…) vit
 * dans QSettings (registre Windows / fichier INI), donc HORS du dossier de
 * données. Ces helpers la sérialisent en JSON pour :
 *   - l'inclure dans la sauvegarde complète (fichier app_config.json déposé dans
 *     le dossier de données avant l'archivage) ;
 *   - offrir une sauvegarde/restauration de la configuration SEULE.
 */

#pragma once
#include <QString>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>

namespace chui {

// Nom fixe du fichier de config déposé dans le dossier de données.
inline QString configPathInDir(const QString& dataDir) { return dataDir + "/app_config.json"; }

// Écrit toutes les clés de configuration dans un fichier JSON. Valeurs stockées
// en texte : la relecture via QVariant::toBool()/toInt()/toString() reste correcte.
inline bool exportConfig(const QString& path) {
    QSettings s("morfredus", "ComponentHub");
    QJsonObject obj;
    for (const QString& k : s.allKeys()) obj[k] = s.value(k).toString();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
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
