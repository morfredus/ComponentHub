/*
 * morfUpdate - durable operation journal
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/OperationStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <utility>

namespace morfupdate {
namespace {

QString fileName(const QString& directory) {
    return QDir(directory).filePath(QStringLiteral("operations.json"));
}

UpdateState stateFromName(const QString& value, bool* valid) {
    static const QHash<QString, UpdateState> states = {
        {QStringLiteral("queued"), UpdateState::Queued},
        {QStringLiteral("downloading"), UpdateState::Downloading},
        {QStringLiteral("verifying"), UpdateState::Verifying},
        {QStringLiteral("installing"), UpdateState::Installing},
        {QStringLiteral("restarting"), UpdateState::Restarting},
        {QStringLiteral("health_check"), UpdateState::HealthCheck},
        {QStringLiteral("succeeded"), UpdateState::Succeeded},
        {QStringLiteral("rejected"), UpdateState::Rejected},
        {QStringLiteral("failed"), UpdateState::Failed},
        {QStringLiteral("rollback_prepared"), UpdateState::RollbackPrepared},
        {QStringLiteral("delegated"), UpdateState::Delegated},
        {QStringLiteral("rolled_back"), UpdateState::RolledBack},
    };
    const auto it = states.constFind(value);
    *valid = it != states.constEnd();
    return *valid ? it.value() : UpdateState::Failed;
}

} // namespace

OperationStore::OperationStore(QString stateDirectory, QObject* parent)
    : QObject(parent), m_stateDirectory(std::move(stateDirectory)) {}

bool OperationStore::load(QString* error) {
    QMutexLocker locker(&m_mutex);
    m_operations.clear();
    m_activeId.clear();
    QFile file(fileName(m_stateDirectory));
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error) *error = QStringLiteral("operation journal is not a JSON object");
        return false;
    }
    const QJsonObject root = document.object();
    for (const QJsonValue& value : root.value(QStringLiteral("operations")).toArray()) {
        UpdateOperation operation;
        if (!fromJson(value.toObject(), &operation)) {
            if (error) *error = QStringLiteral("operation journal contains an invalid entry");
            return false;
        }
        if (!isFinal(operation.state)) {
            if (operation.selfUpdate) {
                // Self-update : le processus qui orchestrait a disparu PAR
                // CONCEPTION (il s'est remplace lui-meme). Ne pas conclure a
                // l'echec : laisser l'operation en l'etat pour la passe de
                // reconciliation (reconcileSelfUpdates), qui tranchera d'apres la
                // version reellement executee. On garde l'id actif pour bloquer
                // tout nouvel update tant que le verdict n'est pas rendu.
                m_activeId = operation.id;
            } else {
                // Un autre service : morfUpdate est reste vivant tout du long, une
                // interruption est donc un vrai echec. Le signaler honnetement et
                // ne pas laisser une seconde mise a jour demarrer avant qu'un
                // operateur l'ait vu.
                operation.state = UpdateState::Failed;
                operation.detail = QStringLiteral("agent interrupted during this operation");
                operation.updatedAt = QDateTime::currentDateTimeUtc();
            }
        }
        m_operations.insert(operation.id, operation);
    }
    // Relacher le handle de lecture AVANT de reecrire : sur Windows, QSaveFile ne
    // peut pas remplacer atomiquement un fichier encore ouvert (« acces refuse »).
    // Sous Linux le rename sur un fichier ouvert passe, d'ou un bug invisible en
    // prod (Pi) mais reel des que morfUpdate tourne sous Windows.
    file.close();
    return save(error);
}

std::optional<UpdateOperation> OperationStore::active() const {
    QMutexLocker locker(&m_mutex);
    if (m_activeId.isEmpty())
        return std::nullopt;
    const auto it = m_operations.constFind(m_activeId);
    if (it == m_operations.constEnd())
        return std::nullopt;
    return it.value();
}

std::optional<UpdateOperation> OperationStore::find(const QString& id) const {
    QMutexLocker locker(&m_mutex);
    const auto it = m_operations.constFind(id);
    if (it == m_operations.constEnd())
        return std::nullopt;
    return it.value();
}

UpdateOperation OperationStore::create(QString project, QString fromVersion, QString toVersion,
                                       QString platform, bool selfUpdate, QString* error) {
    QMutexLocker locker(&m_mutex);
    if (active()) {
        if (error) *error = QStringLiteral("another update is active");
        return {};
    }
    UpdateOperation operation;
    operation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    operation.project = std::move(project);
    operation.fromVersion = std::move(fromVersion);
    operation.toVersion = std::move(toVersion);
    operation.platform = std::move(platform);
    operation.selfUpdate = selfUpdate;
    operation.createdAt = QDateTime::currentDateTimeUtc();
    operation.updatedAt = operation.createdAt;
    m_operations.insert(operation.id, operation);
    m_activeId = operation.id;
    if (!save(error)) {
        m_operations.remove(operation.id);
        m_activeId.clear();
        return {};
    }
    return operation;
}

bool OperationStore::transition(const QString& id, UpdateState state, QString detail,
                                QString* error) {
    QMutexLocker locker(&m_mutex);
    auto it = m_operations.find(id);
    if (it == m_operations.end()) {
        if (error) *error = QStringLiteral("unknown operation");
        return false;
    }
    if (!canTransition(it->state, state)) {
        if (error) *error = QStringLiteral("invalid state transition");
        return false;
    }
    it->state = state;
    it->detail = std::move(detail);
    it->updatedAt = QDateTime::currentDateTimeUtc();
    if (isFinal(state))
        m_activeId.clear();
    return save(error);
}

bool OperationStore::setSelfUpdateRefs(const QString& id, const QString& rollbackRef,
                                       const QString& stagedRef, QString* error) {
    QMutexLocker locker(&m_mutex);
    auto it = m_operations.find(id);
    if (it == m_operations.end()) {
        if (error) *error = QStringLiteral("unknown operation");
        return false;
    }
    it->selfUpdate = true;
    it->rollbackRef = rollbackRef;
    it->stagedRef = stagedRef;
    it->updatedAt = QDateTime::currentDateTimeUtc();
    return save(error);
}

int OperationStore::reconcileSelfUpdates(const QString& runningVersion, QString* error) {
    QMutexLocker locker(&m_mutex);
    // Le processus courant tourne : par definition il est "vivant". Le distinguo
    // Succeeded / RolledBack vient de la version, pas d'une sonde HTTP vers
    // soi-meme. (La porte de sante du redemarrage est portee par l'applieur
    // detache, avant meme que ce successeur ne demarre - voir l'evolution.)
    const bool healthy = true;
    int reconciled = 0;
    // Copie des ids : transition() reprend le verrou (recursif) et modifie la map.
    const QList<QString> ids = m_operations.keys();
    for (const QString& id : ids) {
        const auto it = m_operations.constFind(id);
        if (it == m_operations.constEnd() || !it->selfUpdate || isFinal(it->state))
            continue;
        const UpdateState verdict = reconcileSelfUpdate(it.value(), healthy, runningVersion);
        const QString detail = verdict == UpdateState::Succeeded
            ? QStringLiteral("successor runs the requested version %1").arg(runningVersion)
            : verdict == UpdateState::RolledBack
                ? QStringLiteral("applier restored the previous version %1").arg(runningVersion)
                : QStringLiteral("successor runs an unexpected version %1 (expected %2)")
                      .arg(runningVersion, it->toVersion);
        // transition() valide la transition et persiste. Filet : si le verdict
        // n'est pas atteignable depuis l'etat courant (ne devrait pas arriver),
        // retomber sur Failed, toujours autorise depuis un etat non final.
        QString ignored;
        if (!transition(id, verdict, detail, &ignored))
            transition(id, UpdateState::Failed, detail, &ignored);
        ++reconciled;
    }
    if (reconciled > 0 && !save(error))
        return -1;
    return reconciled;
}

bool OperationStore::save(QString* error) {
    if (!QDir().mkpath(m_stateDirectory)) {
        if (error) *error = QStringLiteral("cannot create operation state directory");
        return false;
    }
    QJsonArray operations;
    for (const UpdateOperation& operation : m_operations)
        operations.append(toJson(operation));
    QSaveFile file(fileName(m_stateDirectory));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("operations"), operations}})
                   .toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QJsonObject OperationStore::toJson(const UpdateOperation& operation) {
    return {
        {QStringLiteral("id"), operation.id},
        {QStringLiteral("project"), operation.project},
        {QStringLiteral("from_version"), operation.fromVersion},
        {QStringLiteral("to_version"), operation.toVersion},
        {QStringLiteral("platform"), operation.platform},
        {QStringLiteral("state"), updateStateName(operation.state)},
        {QStringLiteral("detail"), operation.detail},
        {QStringLiteral("created_at"), operation.createdAt.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("updated_at"), operation.updatedAt.toUTC().toString(Qt::ISODate)},
        // Champs self-update. Toujours ecrits (defaut false / vide) : un journal
        // ecrit par une version anterieure les omet, fromJson retombe alors sur
        // les defauts - compat descendante assuree.
        {QStringLiteral("self_update"), operation.selfUpdate},
        {QStringLiteral("rollback_ref"), operation.rollbackRef},
        {QStringLiteral("staged_ref"), operation.stagedRef},
    };
}

bool OperationStore::fromJson(const QJsonObject& object, UpdateOperation* operation) {
    bool validState = false;
    const UpdateState state = stateFromName(object.value(QStringLiteral("state")).toString(), &validState);
    const QDateTime created = QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
    const QDateTime updated = QDateTime::fromString(object.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);
    if (!validState || object.value(QStringLiteral("id")).toString().isEmpty()
        || object.value(QStringLiteral("project")).toString().isEmpty()
        || !created.isValid() || !updated.isValid())
        return false;
    operation->id = object.value(QStringLiteral("id")).toString();
    operation->project = object.value(QStringLiteral("project")).toString();
    operation->fromVersion = object.value(QStringLiteral("from_version")).toString();
    operation->toVersion = object.value(QStringLiteral("to_version")).toString();
    operation->platform = object.value(QStringLiteral("platform")).toString();
    operation->state = state;
    operation->detail = object.value(QStringLiteral("detail")).toString();
    operation->createdAt = created.toUTC();
    operation->updatedAt = updated.toUTC();
    // Absents d'un journal ancien : defauts (mise a jour normale d'un service).
    operation->selfUpdate = object.value(QStringLiteral("self_update")).toBool(false);
    operation->rollbackRef = object.value(QStringLiteral("rollback_ref")).toString();
    operation->stagedRef = object.value(QStringLiteral("staged_ref")).toString();
    return true;
}

} // namespace morfupdate
