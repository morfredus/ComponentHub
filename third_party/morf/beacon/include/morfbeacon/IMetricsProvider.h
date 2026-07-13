/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QJsonObject>
#include <QString>
#include <functional>
#include <utility>

namespace morfbeacon {

// -----------------------------------------------------------------------------
// IMetricsProvider : point d'extension par lequel chaque application fournit
// SES metriques et SON etat. La bibliotheque ne connait pas la nature des
// metriques : elle se contente de les serialiser dans /status.
//
//   - state()   : "ok" | "warning" | "error" | "starting" (annonce ET status)
//   - metrics() : objet JSON libre, expose UNIQUEMENT via HTTP /status
//                 (jamais dans le heartbeat, qui reste minimal).
// -----------------------------------------------------------------------------
class IMetricsProvider {
public:
    virtual ~IMetricsProvider() = default;

    // Metriques detaillees, propres a l'application (CPU, files, scans...).
    virtual QJsonObject metrics() const = 0;

    // Etat de sante synthetique. Redefinir pour refleter l'etat reel.
    virtual QString state() const { return QStringLiteral("ok"); }
};

// -----------------------------------------------------------------------------
// FunctionMetricsProvider : implementation prete a l'emploi a partir de
// lambdas, pour integrer morfBeacon sans creer de sous-classe.
// -----------------------------------------------------------------------------
class FunctionMetricsProvider : public IMetricsProvider {
public:
    using MetricsFn = std::function<QJsonObject()>;
    using StateFn   = std::function<QString()>;

    explicit FunctionMetricsProvider(MetricsFn metrics, StateFn state = {})
        : m_metrics(std::move(metrics)), m_state(std::move(state)) {}

    QJsonObject metrics() const override {
        return m_metrics ? m_metrics() : QJsonObject{};
    }
    QString state() const override {
        return m_state ? m_state() : QStringLiteral("ok");
    }

private:
    MetricsFn m_metrics;
    StateFn   m_state;
};

} // namespace morfbeacon
