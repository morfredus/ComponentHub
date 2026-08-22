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
    };
    const auto it = states.constFind(value);
    *valid = it != states.constEnd();
    return *valid ? it.value() : UpdateState::Failed;
}

} // namespace

OperationStore::OperationStore(QString stateDirectory, QObject* parent)
    : QObject(parent), m_stateDirectory(std::move(stateDirectory)) {}

bool OperationStore::load(QString* error) {
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
            // A previous process died mid-operation.  It cannot know whether an
            // installer finished, so report the interruption honestly and do
            // not let a second update start until an operator has seen it.
            operation.state = UpdateState::Failed;
            operation.detail = QStringLiteral("agent interrupted during this operation");
            operation.updatedAt = QDateTime::currentDateTimeUtc();
        }
        m_operations.insert(operation.id, operation);
    }
    return save(error);
}

const UpdateOperation* OperationStore::active() const {
    if (m_activeId.isEmpty())
        return nullptr;
    const auto it = m_operations.constFind(m_activeId);
    return it == m_operations.constEnd() ? nullptr : &it.value();
}

const UpdateOperation* OperationStore::find(const QString& id) const {
    const auto it = m_operations.constFind(id);
    return it == m_operations.constEnd() ? nullptr : &it.value();
}

UpdateOperation OperationStore::create(QString project, QString fromVersion, QString toVersion,
                                       QString platform, QString* error) {
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
    return true;
}

} // namespace morfupdate
