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
    }
    return {};
}

bool isFinal(UpdateState state) {
    return state == UpdateState::Succeeded || state == UpdateState::Rejected
        || state == UpdateState::Failed;
}

bool canTransition(UpdateState from, UpdateState to) {
    if (isFinal(from))
        return false;
    if (to == UpdateState::Rejected || to == UpdateState::Failed)
        return true;
    switch (from) {
    case UpdateState::Queued:      return to == UpdateState::Downloading;
    case UpdateState::Downloading: return to == UpdateState::Verifying;
    case UpdateState::Verifying:   return to == UpdateState::Installing;
    case UpdateState::Installing:  return to == UpdateState::Restarting;
    case UpdateState::Restarting:  return to == UpdateState::HealthCheck;
    case UpdateState::HealthCheck: return to == UpdateState::Succeeded;
    default:                        return false;
    }
}

} // namespace morfupdate
