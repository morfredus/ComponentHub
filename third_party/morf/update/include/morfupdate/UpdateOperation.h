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
};

} // namespace morfupdate
