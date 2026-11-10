/**
 * AppConfigIO.h - Backup / restore of the application configuration.
 *
 * The configuration (hub address, token, synchronization options, appearance...) lives
 * in QSettings (Windows registry / INI file), therefore OUTSIDE the data
 * folder. These helpers serialize it to JSON in order to:
 *   - include it in the full backup (app_config.json, WITHOUT the token, added
 *     directly to the archive: no config file stays in the data folder);
 *   - offer a backup/restore of the configuration ALONE.
 */

#pragma once
#include <QString>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QByteArray>

namespace chui {

// Fixed name of the config file placed in the data folder.
inline QString configPathInDir(const QString& dataDir) { return dataDir + "/app_config.json"; }

// Serializes every configuration key to JSON. Values are stored as text: reading
// them back through QVariant::toBool()/toInt()/toString() stays correct. The synchronization token
// (sync/token) is only included on request: a backup archived and then dropped on a
// share must not carry a secret. Reminder: this token travels over plain HTTP to the hub,
// so it should be reserved for a trusted LAN.
inline QByteArray configJson(bool includeToken) {
    QSettings s("morfredus", "ComponentHub");
    QJsonObject obj;
    for (const QString& k : s.allKeys()) {
        if (!includeToken && k == "sync/token") continue;
        obj[k] = s.value(k).toString();
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Indented);
}

// Writes the configuration into a JSON file.
inline bool exportConfig(const QString& path, bool includeToken = true) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(configJson(includeToken));
    return true;
}

// Reloads the configuration from a JSON file into QSettings.
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
