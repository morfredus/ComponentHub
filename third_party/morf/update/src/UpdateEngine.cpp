#include "morfupdate/UpdateEngine.h"

#include "morfupdate/OperationStore.h"
#include "morfupdate/ReleaseValidator.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

namespace morfupdate {
namespace {

QString platformName() {
#ifdef Q_OS_WIN
    return QStringLiteral("windows-x86_64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("linux-arm64");
#else
    return QStringLiteral("linux-amd64");
#endif
}

QByteArray get(const QUrl& url, const QByteArray& token, QString* error, bool download = false) {
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", download ? "application/octet-stream" : "application/vnd.github+json");
    request.setRawHeader("User-Agent", "morfUpdate-agent");
    if (!token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + token);
    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(30000);
    loop.exec();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        *error = QStringLiteral("release request failed: ") + reply->errorString();
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();
    return body;
}

bool jsonGet(const QString& apiPath, const QByteArray& token, QJsonObject* result, QString* error) {
    const QByteArray raw = get(QUrl(QStringLiteral("https://api.github.com/") + apiPath), token, error);
    if (raw.isEmpty()) return false;
    const QJsonDocument document = QJsonDocument::fromJson(raw);
    if (!document.isObject()) {
        *error = QStringLiteral("GitHub response is not a JSON object");
        return false;
    }
    *result = document.object();
    return true;
}

bool taggedCommit(const QString& repository, const QString& version, const QByteArray& token,
                  QString* commit, QString* error) {
    QJsonObject ref;
    if (!jsonGet(QStringLiteral("repos/") + repository + QStringLiteral("/git/ref/tags/v") + version,
                 token, &ref, error)) return false;
    QJsonObject object = ref.value("object").toObject();
    while (object.value("type").toString() == QStringLiteral("tag")) {
        QJsonObject tag;
        if (!jsonGet(QStringLiteral("repos/") + repository + QStringLiteral("/git/tags/")
                     + object.value("sha").toString(), token, &tag, error)) return false;
        object = tag.value("object").toObject();
    }
    if (object.value("type").toString() != QStringLiteral("commit")
        || object.value("sha").toString().size() != 40) {
        *error = QStringLiteral("release tag does not resolve to a full commit");
        return false;
    }
    *commit = object.value("sha").toString();
    return true;
}

bool runProcess(const QString& program, const QStringList& arguments, QString* error) {
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(10000) || !process.waitForFinished(-1)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString output = QString::fromUtf8(process.readAllStandardError()).trimmed();
        *error = output.isEmpty() ? QStringLiteral("privileged helper failed") : output;
        return false;
    }
    return true;
}

bool healthCheck(const QString& url, QString* error) {
    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(QNetworkRequest(QUrl(url)));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(15000);
    loop.exec();
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
    if (!ok) *error = QStringLiteral("service health check failed");
    reply->deleteLater();
    return ok;
}

bool installLinux(const QString& file, const AgentTarget& target, QString* error) {
    return runProcess(QStringLiteral("/usr/lib/morfsystem/morfupdate/morfupdate-helper"),
                      {QStringLiteral("--install-deb"), file, target.service}, error);
}

#ifdef Q_OS_WIN
bool safeArchive(const QString& zip, QStringList* entries, QString* error) {
    QProcess tar;
    tar.start(QStringLiteral("tar.exe"), {QStringLiteral("-tf"), zip});
    if (!tar.waitForFinished(30000) || tar.exitCode() != 0) {
        *error = QStringLiteral("cannot inspect ZIP archive");
        return false;
    }
    *entries = QString::fromUtf8(tar.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const QString& entry : *entries) {
        if (entry.startsWith('/') || entry.contains(QStringLiteral("..")) || entry.contains(':')) {
            *error = QStringLiteral("ZIP archive contains an unsafe path");
            return false;
        }
    }
    return !entries->isEmpty();
}

bool installWindows(const QString& zip, const AgentTarget& target, const QString& stage, QString* error) {
    if (target.appDir.isEmpty()) {
        *error = QStringLiteral("Windows target has no declared application directory");
        return false;
    }
    QStringList entries;
    if (!safeArchive(zip, &entries, error)) return false;
    const QString unpack = QDir(stage).filePath(QStringLiteral("unpack"));
    if (!QDir().mkpath(unpack)
        || !runProcess(QStringLiteral("tar.exe"), {QStringLiteral("-xf"), zip, QStringLiteral("-C"), unpack}, error))
        return false;
    const QString payload = QDir(unpack).filePath(target.service);
    if (!QDir(payload).exists()) {
        *error = QStringLiteral("ZIP archive does not contain the declared service directory");
        return false;
    }
    const bool task = target.serviceManager == QStringLiteral("task");
    if (!runProcess(task ? QStringLiteral("schtasks.exe") : QStringLiteral("sc.exe"),
                    task ? QStringList{QStringLiteral("/End"), QStringLiteral("/TN"), target.service}
                         : QStringList{QStringLiteral("stop"), target.service}, error)) return false;
    const QString backup = QDir(stage).filePath(QStringLiteral("previous"));
    if (QDir(target.appDir).exists() && !QDir().rename(target.appDir, backup)) {
        *error = QStringLiteral("cannot preserve current Windows application directory");
        return false;
    }
    if (!QDir().rename(payload, target.appDir)) {
        *error = QStringLiteral("cannot install verified Windows application directory");
        return false;
    }
    return runProcess(task ? QStringLiteral("schtasks.exe") : QStringLiteral("sc.exe"),
                      task ? QStringList{QStringLiteral("/Run"), QStringLiteral("/TN"), target.service}
                           : QStringList{QStringLiteral("start"), target.service}, error);
}
#endif

} // namespace

UpdateEngine::UpdateEngine(AgentConfig config, OperationStore* operations, QString stateDirectory,
                           QObject* parent)
    : QObject(parent), m_config(std::move(config)), m_operations(operations),
      m_stateDirectory(std::move(stateDirectory)) {}

bool UpdateEngine::fail(const QString& operationId, const QString& detail) {
    QString ignored;
    return m_operations->transition(operationId, UpdateState::Failed, detail, &ignored);
}

void UpdateEngine::run(const QString& operationId) {
    const UpdateOperation* operation = m_operations->find(operationId);
    if (!operation || operation->state != UpdateState::Queued) return;
    const AgentTarget target = m_config.targets.value(operation->project);
    if (target.project.isEmpty()) { fail(operationId, QStringLiteral("project is not configured")); return; }
    const QByteArray token;
    QString error;
    if (!m_operations->transition(operationId, UpdateState::Downloading, QStringLiteral("reading release"), &error)) return;
    QJsonObject release;
    if (!jsonGet(QStringLiteral("repos/") + target.repository + QStringLiteral("/releases/tags/v")
                 + operation->toVersion, token, &release, &error)) { fail(operationId, error); return; }
    QString commit;
    if (!taggedCommit(target.repository, operation->toVersion, token, &commit, &error)) { fail(operationId, error); return; }
    QJsonObject manifestAsset;
    for (const QJsonValue& value : release.value("assets").toArray()) {
        const QJsonObject asset = value.toObject();
        if (asset.value("name").toString() == QStringLiteral("manifest.json")) manifestAsset = asset;
    }
    if (manifestAsset.isEmpty()) { fail(operationId, QStringLiteral("release has no manifest.json")); return; }
    const QByteArray rawManifest = get(QUrl(manifestAsset.value("url").toString()), token, &error, true);
    const QJsonDocument manifestDoc = QJsonDocument::fromJson(rawManifest);
    if (!manifestDoc.isObject()) { fail(operationId, error.isEmpty() ? QStringLiteral("manifest is invalid") : error); return; }
    if (manifestDoc.object().value("source").toObject().value("repository").toString()
        != target.repository) {
        fail(operationId, QStringLiteral("manifest declares a different source repository"));
        return;
    }
    ValidatedAsset asset;
    if (!ReleaseValidator::selectAsset(manifestDoc.object(), operation->project, operation->toVersion,
                                       operation->platform, commit, &asset, &error)) { fail(operationId, error); return; }
    const QString expectedFormat = operation->platform.startsWith(QStringLiteral("windows")) ? "zip" : "deb";
    if (asset.format != expectedFormat) { fail(operationId, QStringLiteral("manifest asset format is incompatible")); return; }
    QJsonObject artifactAsset;
    for (const QJsonValue& value : release.value("assets").toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value("name").toString() == asset.name) artifactAsset = candidate;
    }
    if (artifactAsset.isEmpty()) { fail(operationId, QStringLiteral("manifest asset is absent from release")); return; }
    const QString stage = QDir(m_stateDirectory).filePath(QStringLiteral("downloads/") + operationId);
    if (!QDir().mkpath(stage)) { fail(operationId, QStringLiteral("cannot create protected download directory")); return; }
    const QString file = QDir(stage).filePath(asset.name);
    const QByteArray bytes = get(QUrl(artifactAsset.value("url").toString()), token, &error, true);
    QSaveFile downloaded(file);
    if (bytes.isEmpty() || !downloaded.open(QIODevice::WriteOnly) || downloaded.write(bytes) != bytes.size()
        || !downloaded.commit()) { fail(operationId, error.isEmpty() ? QStringLiteral("cannot save release asset") : error); return; }
    if (!m_operations->transition(operationId, UpdateState::Verifying, QStringLiteral("checking SHA-256 and provenance"), &error)
        || !ReleaseValidator::checksumMatches(file, asset.sha256, &error)) { fail(operationId, error); return; }
    if (!m_operations->transition(operationId, UpdateState::Installing, QStringLiteral("installing verified package"), &error)) return;
#ifdef Q_OS_WIN
    if (!installWindows(file, target, stage, &error)) { fail(operationId, error); return; }
#else
    if (!installLinux(file, target, &error)) { fail(operationId, error); return; }
#endif
    if (!m_operations->transition(operationId, UpdateState::Restarting, QStringLiteral("service restarted"), &error)) return;
    if (!m_operations->transition(operationId, UpdateState::HealthCheck, QStringLiteral("checking service health"), &error)
        || !healthCheck(target.healthUrl, &error)) { fail(operationId, error); return; }
    m_operations->transition(operationId, UpdateState::Succeeded, QStringLiteral("installed version passed health check"), &error);
}

} // namespace morfupdate
