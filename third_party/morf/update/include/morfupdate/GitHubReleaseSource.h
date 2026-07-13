/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include "morfupdate/IUpdateSource.h"
#include "morfupdate/morfUpdateConfig.h"

class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;

namespace morfupdate {

// -----------------------------------------------------------------------------
// GitHubReleaseSource : interroge l'API GitHub Releases.
//
//   - sans pre-releases : GET /repos/{owner}/{repo}/releases/latest
//   - avec pre-releases : GET /repos/{owner}/{repo}/releases  (prend la 1re
//                         non-brouillon compatible)
// -----------------------------------------------------------------------------
class GitHubReleaseSource : public IUpdateSource {
    Q_OBJECT
public:
    explicit GitHubReleaseSource(morfUpdateConfig config, QObject* parent = nullptr);
    ~GitHubReleaseSource() override;

    void fetchLatest(bool includePrereleases) override;

private:
    void handleReply(QNetworkReply* reply, bool includePrereleases);
    ReleaseInfo parseRelease(const QJsonObject& obj) const;

    morfUpdateConfig    m_config;
    QNetworkAccessManager* m_nam;
};

} // namespace morfupdate
