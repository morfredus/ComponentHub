/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/UpdateChecker.h"
#include "morfupdate/IUpdateSource.h"
#include "morfupdate/GitHubReleaseSource.h"
#include "morfupdate/Version.h"

#include <utility>

namespace morfupdate {

UpdateChecker::UpdateChecker(morfUpdateConfig config, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_source(new GitHubReleaseSource(m_config, this)) {
    wire();
}

UpdateChecker::UpdateChecker(morfUpdateConfig config, IUpdateSource* source, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_source(source) {
    if (m_source && !m_source->parent())
        m_source->setParent(this);
    wire();
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::wire() {
    connect(m_source, &IUpdateSource::latestReady, this,
            [this](const ReleaseInfo& info) {
                const Version current = Version::parse(m_config.currentVersion);
                if (info.version > current)
                    emit updateAvailable(info);
                else
                    emit upToDate(info);
            });

    connect(m_source, &IUpdateSource::failed, this,
            [this](const QString& err) { emit checkFailed(err); });
}

void UpdateChecker::checkForUpdates() {
    m_source->fetchLatest(m_config.includePrereleases);
}

QString UpdateChecker::currentVersion() const {
    return m_config.currentVersion;
}

} // namespace morfupdate
