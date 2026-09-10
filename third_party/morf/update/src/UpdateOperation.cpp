/*
 * morfUpdate - persistent, explicit update operations
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/UpdateOperation.h"

namespace morfupdate {

QString updateStateName(UpdateState state) {
    switch (state) {
    case UpdateState::Queued:      return QStringLiteral("queued");
    case UpdateState::Downloading: return QStringLiteral("downloading");
    case UpdateState::Verifying:   return QStringLiteral("verifying");
    case UpdateState::Installing:  return QStringLiteral("installing");
    case UpdateState::Restarting:  return QStringLiteral("restarting");
    case UpdateState::HealthCheck: return QStringLiteral("health_check");
    case UpdateState::Succeeded:   return QStringLiteral("succeeded");
    case UpdateState::Rejected:    return QStringLiteral("rejected");
    case UpdateState::Failed:      return QStringLiteral("failed");
    case UpdateState::RollbackPrepared: return QStringLiteral("rollback_prepared");
    case UpdateState::Delegated:   return QStringLiteral("delegated");
    case UpdateState::RolledBack:  return QStringLiteral("rolled_back");
    }
    return {};
}

bool isFinal(UpdateState state) {
    return state == UpdateState::Succeeded || state == UpdateState::Rejected
        || state == UpdateState::Failed || state == UpdateState::RolledBack;
}

bool canTransition(UpdateState from, UpdateState to) {
    if (isFinal(from))
        return false;
    if (to == UpdateState::Rejected || to == UpdateState::Failed)
        return true;
    switch (from) {
    // Downloading = mise a jour (telechargement, verif, install). Restarting =
    // relance directe (bouton « Relancer » d'un service bloque) : ni source, ni
    // fichier, on saute droit au redemarrage.
    case UpdateState::Queued:      return to == UpdateState::Downloading
                                          || to == UpdateState::Restarting;
    case UpdateState::Downloading: return to == UpdateState::Verifying;
    // Verifying -> Installing : flux normal. Verifying -> RollbackPrepared :
    // bifurcation self-update (on stashe l'ancien paquet avant de deleguer).
    case UpdateState::Verifying:   return to == UpdateState::Installing
                                          || to == UpdateState::RollbackPrepared;
    case UpdateState::Installing:  return to == UpdateState::Restarting;
    case UpdateState::Restarting:  return to == UpdateState::HealthCheck;
    case UpdateState::HealthCheck: return to == UpdateState::Succeeded;
    // Chemin self-update : preparation -> delegation -> verdict du successeur.
    case UpdateState::RollbackPrepared: return to == UpdateState::Delegated
                                               || to == UpdateState::RolledBack;
    case UpdateState::Delegated:   return to == UpdateState::Succeeded
                                          || to == UpdateState::RolledBack;
    default:                        return false;
    }
}

UpdateState reconcileSelfUpdate(const UpdateOperation& op, bool healthy,
                                const QString& runningVersion) {
    if (!healthy)
        return UpdateState::Failed;
    if (runningVersion == op.toVersion)
        return UpdateState::Succeeded;
    if (runningVersion == op.fromVersion)
        return UpdateState::RolledBack;
    return UpdateState::Failed;
}

} // namespace morfupdate
