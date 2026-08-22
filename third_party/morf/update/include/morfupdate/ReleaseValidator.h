#pragma once

#include <QJsonObject>
#include <QString>

namespace morfupdate {

struct ValidatedAsset {
    QString name;
    QString sha256;
    QString commit;
    QString format;
    QString platform;
};

// Parses the manifest copied to the project release. Network retrieval belongs
// to the agent worker; keeping the contract validation pure makes it testable
// and prevents either platform backend from interpreting untrusted JSON.
class ReleaseValidator final {
public:
    static bool selectAsset(const QJsonObject& manifest, const QString& project,
                            const QString& version, const QString& platform,
                            const QString& taggedCommit, ValidatedAsset* asset,
                            QString* error);
    static bool checksumMatches(const QString& path, const QString& expected,
                                QString* error);
};

} // namespace morfupdate
