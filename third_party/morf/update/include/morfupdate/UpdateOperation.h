/*
 * morfUpdate - persistent, explicit update operations
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDateTime>
#include <QString>

namespace morfupdate {

// The states are part of the HTTP contract.  Keep their wire values stable:
// morfMonitor persists and displays them without interpreting installer output.
//
// RollbackPrepared / Delegated / RolledBack sont additifs et servent UNIQUEMENT
// au chemin self-update (morfUpdate se met a jour lui-meme). Ils ne sont jamais
// emis pour la mise a jour d'un autre service : le flux existant est inchange.
// Voir Evolution/morfUpdate - auto-mise a jour (succession de processus).md.
enum class UpdateState {
    Queued,
    Downloading,
    Verifying,
    Installing,
    Restarting,
    HealthCheck,
    Succeeded,
    Rejected,
    Failed,
    RollbackPrepared,  // self-update : ancien paquet stashe, nouveau paquet stage
    Delegated,         // self-update : applieur detache lance ; le processus peut mourir
    RolledBack,        // self-update terminal : l'ancienne version a ete restauree
};

QString updateStateName(UpdateState state);
bool isFinal(UpdateState state);
bool canTransition(UpdateState from, UpdateState to);

struct UpdateOperation {
    QString id;
    QString project;
    QString fromVersion;
    QString toVersion;
    QString platform;
    UpdateState state = UpdateState::Queued;
    QString detail;
    QDateTime createdAt;
    QDateTime updatedAt;
    // Champs self-update (defaut : mise a jour normale d'un autre service).
    bool    selfUpdate = false;  // morfUpdate est sa propre cible
    QString rollbackRef;         // reference du paquet stashe pour le rollback
    QString stagedRef;           // reference du nouveau paquet prepare
};

// Verdict du successeur pour une operation self-update reprise apres la
// disparition du processus qui l'orchestrait. L'installation n'est PAS le
// succes : seule la version reellement executee (et la sante) tranche.
//   healthy && running == toVersion   -> Succeeded
//   healthy && running == fromVersion -> RolledBack (l'applieur a restaure)
//   sinon                             -> Failed
UpdateState reconcileSelfUpdate(const UpdateOperation& op, bool healthy,
                                const QString& runningVersion);

} // namespace morfupdate
