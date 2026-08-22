#include "morfupdate/ReleaseValidator.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace morfupdate {
namespace {
const QRegularExpression kSha256(QStringLiteral("^[0-9a-f]{64}$"));
const QRegularExpression kCommit(QStringLiteral("^[0-9a-f]{40}$"));

bool platformMatches(const QJsonObject& platform, const QString& expected) {
    const QString os = platform.value(QStringLiteral("os")).toString();
    const QString arch = platform.value(QStringLiteral("arch")).toString();
    if (expected == QStringLiteral("linux-amd64")) return os == "linux" && arch == "x86_64";
    if (expected == QStringLiteral("linux-arm64")) return os == "linux" && arch == "arm64";
    if (expected == QStringLiteral("windows-x86_64")) return os == "windows" && arch == "x86_64";
    return false;
}
}

bool ReleaseValidator::selectAsset(const QJsonObject& manifest, const QString& project,
                                   const QString& version, const QString& platform,
                                   const QString& taggedCommit, ValidatedAsset* asset,
                                   QString* error) {
    if (manifest.value("schema_version").toInt() != 1
        || manifest.value("project").toString() != project
        || manifest.value("version").toString() != version) {
        if (error) *error = QStringLiteral("manifest does not identify the requested release");
        return false;
    }
    const QJsonObject source = manifest.value("source").toObject();
    if (!kCommit.match(taggedCommit).hasMatch()
        || source.value("tag").toString() != QStringLiteral("v") + version
        || source.value("commit").toString() != taggedCommit) {
        if (error) *error = QStringLiteral("manifest source does not match the release tag");
        return false;
    }
    ValidatedAsset selected;
    int matches = 0;
    for (const QJsonValue& value : manifest.value("artifacts").toArray()) {
        const QJsonObject entry = value.toObject();
        if (!platformMatches(entry.value("platform").toObject(), platform))
            continue;
        const QString name = entry.value("name").toString();
        const QString sha = entry.value("sha256").toString();
        const QString commit = entry.value("commit").toString();
        const QString format = entry.value("format").toString();
        if (name.isEmpty() || !kSha256.match(sha).hasMatch() || commit != taggedCommit
            || format.isEmpty()) {
            if (error) *error = QStringLiteral("manifest contains an invalid platform asset");
            return false;
        }
        selected = {name, sha, commit, format, platform};
        ++matches;
    }
    if (matches != 1) {
        if (error) *error = matches ? QStringLiteral("multiple assets match this platform")
                                    : QStringLiteral("no asset matches this platform");
        return false;
    }
    *asset = selected;
    return true;
}

bool ReleaseValidator::checksumMatches(const QString& path, const QString& expected,
                                       QString* error) {
    if (!kSha256.match(expected).hasMatch()) {
        if (error) *error = QStringLiteral("invalid expected SHA-256");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        if (!hash.addData(&file)) {
            if (error) *error = QStringLiteral("cannot read downloaded asset");
            return false;
        }
    }
    if (QString::fromLatin1(hash.result().toHex()) != expected) {
        if (error) *error = QStringLiteral("downloaded asset checksum differs from manifest");
        return false;
    }
    return true;
}

} // namespace morfupdate
