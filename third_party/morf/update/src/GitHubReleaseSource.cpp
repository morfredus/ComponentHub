/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/GitHubReleaseSource.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

#include <utility>

namespace morfupdate {

GitHubReleaseSource::GitHubReleaseSource(morfUpdateConfig config, QObject* parent)
    : IUpdateSource(parent),
      m_config(std::move(config)),
      m_nam(new QNetworkAccessManager(this)) {}

GitHubReleaseSource::~GitHubReleaseSource() = default;

void GitHubReleaseSource::fetchLatest(bool includePrereleases) {
    if (m_config.owner.isEmpty() || m_config.repo.isEmpty()) {
        emit failed(QStringLiteral("owner/repo non renseignes"));
        return;
    }

    const QString base = QStringLiteral("https://api.github.com/repos/%1/%2/releases")
                             .arg(m_config.owner, m_config.repo);
    // /latest exclut deja brouillons et pre-releases cote GitHub.
    const QUrl url(includePrereleases ? base + QStringLiteral("?per_page=10")
                                      : base + QStringLiteral("/latest"));

    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    // GitHub exige un User-Agent, sinon 403.
    req.setRawHeader("User-Agent", "morfUpdate-Qt");
    if (!m_config.githubToken.isEmpty())
        req.setRawHeader("Authorization",
                         "Bearer " + m_config.githubToken.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, includePrereleases]() {
        handleReply(reply, includePrereleases);
    });
}

void GitHubReleaseSource::handleReply(QNetworkReply* reply, bool includePrereleases) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(QStringLiteral("Erreur reseau : %1").arg(reply->errorString()));
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError) {
        emit failed(QStringLiteral("Reponse illisible : %1").arg(perr.errorString()));
        return;
    }

    ReleaseInfo info;

    if (doc.isObject()) {
        // Reponse de /latest.
        info = parseRelease(doc.object());
    } else if (doc.isArray()) {
        // Liste : premiere release non-brouillon compatible avec le filtre.
        const QJsonArray arr = doc.array();
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            if (o.value("draft").toBool(false))
                continue;
            if (!includePrereleases && o.value("prerelease").toBool(false))
                continue;
            info = parseRelease(o);
            break;
        }
    }

    if (!info.valid) {
        emit failed(QStringLiteral("Aucune release exploitable trouvee"));
        return;
    }
    emit latestReady(info);
}

ReleaseInfo GitHubReleaseSource::parseRelease(const QJsonObject& obj) const {
    ReleaseInfo info;
    info.tag         = obj.value("tag_name").toString();
    info.name        = obj.value("name").toString();
    info.notes       = obj.value("body").toString();
    info.htmlUrl     = QUrl(obj.value("html_url").toString());
    info.publishedAt = obj.value("published_at").toString();
    info.prerelease  = obj.value("prerelease").toBool(false);
    info.version     = Version::parse(info.tag);

    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue& av : assets) {
        const QJsonObject ao = av.toObject();
        ReleaseAsset a;
        a.name = ao.value("name").toString();
        a.url  = QUrl(ao.value("browser_download_url").toString());
        a.size = static_cast<qint64>(ao.value("size").toDouble());
        info.assets.push_back(a);
    }

    info.valid = info.version.valid && !info.tag.isEmpty();
    return info;
}

} // namespace morfupdate
