/**
 * SyncPrefs.h - Persisted synchronization preferences (QSettings), shared
 * between the Settings page and the main window. A single place for the keys.
 */

#pragma once
#include <QSettings>
#include <QDateTime>
#include <QString>
#include "sync/SyncService.h"

namespace chui {

inline chsync::SyncConfig readSyncConfig() {
    QSettings s("morfredus", "ComponentHub");
    chsync::SyncConfig cfg;
    cfg.serverUrl = s.value("sync/serverUrl", "http://morfsync.local:8080").toString().toStdString();
    cfg.token     = s.value("sync/token", "").toString().toStdString();
    return cfg;
}

// Explicit "local only" mode: the user chooses to use ComponentHub offline.
// NO synchronization (no startup, no button, no close). Sovereign
// local database, fully assumed. Takes priority over everything else.
inline bool syncLocalOnly() {
    return QSettings("morfredus", "ComponentHub").value("sync/localOnly", false).toBool();
}

inline bool syncConfigured() {
    QSettings s("morfredus", "ComponentHub");
    if (syncLocalOnly()) return false;   // pure local: never "configured" for synchronization
    return !s.value("sync/serverUrl", "").toString().trimmed().isEmpty();
}
inline bool syncAutoStart() {
    return QSettings("morfredus", "ComponentHub").value("sync/autoStart", true).toBool();
}
inline bool syncAskOnQuit() {
    return QSettings("morfredus", "ComponentHub").value("sync/askOnQuit", true).toBool();
}

// Stores the timestamp and the result of the last successful synchronization.
inline void recordLastSync(int received, int sent) {
    QSettings s("morfredus", "ComponentHub");
    s.setValue("sync/lastSyncAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    s.setValue("sync/lastReceived", received);
    s.setValue("sync/lastSent", sent);
}

// Readable summary of the last synchronization (empty if never synchronized):
//   Last synchronization:
//   Today at 11:42
//   12 received - 3 sent
inline QString lastSyncSummary() {
    QSettings s("morfredus", "ComponentHub");
    const QString iso = s.value("sync/lastSyncAt", "").toString();
    if (iso.isEmpty()) return QString();
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    const QDate d = dt.date(), today = QDate::currentDate();
    const QString hm = dt.toString("HH:mm");
    QString when;
    if (d == today)                 when = "Aujourd'hui à " + hm;
    else if (d == today.addDays(-1)) when = "Hier à " + hm;
    else                             when = "le " + dt.toString("dd/MM/yyyy") + " à " + hm;
    const int rec  = s.value("sync/lastReceived", 0).toInt();
    const int sent = s.value("sync/lastSent", 0).toInt();
    return QString("Dernière synchronisation :\n%1\n%2 reçus - %3 envoyés").arg(when).arg(rec).arg(sent);
}

} // namespace chui
