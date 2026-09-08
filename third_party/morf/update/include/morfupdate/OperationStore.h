/*
 * morfUpdate - durable operation journal
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "morfupdate/UpdateOperation.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>

#include <optional>

namespace morfupdate {

// OperationStore owns the truth about update progress on one machine.  It has
// no network or installer knowledge: an interrupted process must retain the
// last honest state rather than inventing a successful outcome at restart.
class OperationStore final : public QObject {
    Q_OBJECT
public:
    explicit OperationStore(QString stateDirectory, QObject* parent = nullptr);

    bool load(QString* error = nullptr);
    // Renvoient une COPIE (snapshot) sous verrou, pas un pointeur interne : le
    // moteur d'update tourne sur un thread worker et le serveur HTTP sur le thread
    // principal ; rendre un pointeur vers la QHash partagee serait une course de
    // donnees des que l'un lit pendant que l'autre ecrit. std::nullopt = absent.
    std::optional<UpdateOperation> active() const;
    std::optional<UpdateOperation> find(const QString& id) const;

    // Returns an empty id when another operation is active.  Callers may show
    // active() to the client with HTTP 409, without ever queueing a second
    // installation behind an unknown result.
    UpdateOperation create(QString project, QString fromVersion, QString toVersion,
                           QString platform, QString* error = nullptr);
    bool transition(const QString& id, UpdateState state, QString detail,
                    QString* error = nullptr);

private:
    bool save(QString* error);
    static QJsonObject toJson(const UpdateOperation& operation);
    static bool fromJson(const QJsonObject& object, UpdateOperation* operation);

    QString m_stateDirectory;
    QHash<QString, UpdateOperation> m_operations;
    QString m_activeId;
    // Recursif : create() appelle active(), toutes deux prennent le verrou sur le
    // meme thread. save() est prive et n'est appele que sous verrou (il ne verrouille
    // pas lui-meme). Protege l'acces concurrent thread HTTP <-> thread worker.
    mutable QRecursiveMutex m_mutex;
};

} // namespace morfupdate
