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

    // Etat du MATERIEL geré par le service, exposé UNIQUEMENT via /status (comme
    // metrics()). Contrat additif : un service sans matériel renvoie {} (défaut)
    // et rien n'apparait. Un service à matériel (capteur, écran...) renvoie au
    // moins :
    //   { "state": "present" | "none" | "degraded", "label": "<texte lisible>" }
    // et, s'il le peut, "expected"/"present" (compteurs). Sémantique :
    //   - "present"  : matériel attendu ET disponible ;
    //   - "none"     : AUCUN matériel attendu ici (configuration valide, pas une
    //                  panne) ;
    //   - "degraded" : matériel attendu mais absent ou défaillant.
    // Règle de séparation : chaque service reste SEUL juge de son matériel ; le
    // superviseur (morfMonitor) lit ce bloc et l'affiche SANS jamais déduire la
    // présence lui-même. L'état service (state()) doit rester cohérent : "none"
    // et "present" n'empêchent pas un "ok" ; seul "degraded" justifie un
    // "warning".
    virtual QJsonObject hardware() const { return {}; }
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
