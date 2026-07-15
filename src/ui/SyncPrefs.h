/**
 * SyncPrefs.h — Préférences de synchronisation persistées (QSettings), partagées
 * entre la page Réglages et la fenêtre principale. Un seul endroit pour les clés.
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
    cfg.serverUrl = s.value("sync/serverUrl", "http://homeserverhub.local:8080").toString().toStdString();
    cfg.token     = s.value("sync/token", "").toString().toStdString();
    return cfg;
}

// Mode explicite « local uniquement » : l'utilisateur choisit de n'utiliser
// AUCUNE synchronisation (ni démarrage, ni bouton, ni fermeture). Base locale
// souveraine assumée jusqu'au bout. Prioritaire sur tout le reste.
inline bool syncLocalOnly() {
    return QSettings("morfredus", "ComponentHub").value("sync/localOnly", false).toBool();
}

inline bool syncConfigured() {
    QSettings s("morfredus", "ComponentHub");
    if (syncLocalOnly()) return false;   // local pur : jamais « configuré » pour la synchro
    return !s.value("sync/serverUrl", "").toString().trimmed().isEmpty();
}
inline bool syncAutoStart() {
    return QSettings("morfredus", "ComponentHub").value("sync/autoStart", true).toBool();
}
inline bool syncAskOnQuit() {
    return QSettings("morfredus", "ComponentHub").value("sync/askOnQuit", true).toBool();
}

// Mémorise l'horodatage et le bilan de la dernière synchro réussie.
inline void recordLastSync(int received, int sent) {
    QSettings s("morfredus", "ComponentHub");
    s.setValue("sync/lastSyncAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    s.setValue("sync/lastReceived", received);
    s.setValue("sync/lastSent", sent);
}

// Résumé lisible de la dernière synchro (vide si jamais synchronisé) :
//   Dernière synchronisation :
//   Aujourd'hui à 11:42
//   12 reçus - 3 envoyés
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
