/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QtGlobal>

namespace morfbeacon {

// Une route de l'API métier d'une application, annoncée par /status (voir
// PresenceConfig::api). Le trio method/path/summary suffit à l'inventaire, aux
// liens et à une vue de diagnostic. Volontairement PAS un schéma d'API complet :
// une application qui veut décrire ses paramètres publie son propre document
// OpenAPI et l'annonce comme une interface web ; le beacon ne réimplémente pas
// OpenAPI.
struct ApiEndpoint {
    QString method;    // "GET", "POST"...
    QString path;      // "/api/system"
    QString summary;   // phrase courte, facultative
};

// -----------------------------------------------------------------------------
// PresenceConfig : parametres d'une application supervisee.
//
// Le heartbeat UDP annonce la PRESENCE (peu bavard, periodique). Le serveur
// HTTP local expose le DETAIL (a la demande). Les deux partagent cette config.
// -----------------------------------------------------------------------------
struct PresenceConfig {
    // --- Identite annoncee (LIBRE, modifiable par l'utilisateur) ------------
    QString appName   = QStringLiteral("App");  // ex. "ComponentHub"
    QString version   = QStringLiteral("dev");  // ex. "1.4.2"
    QString instanceId;                         // optionnel ; defaut = appName@hostname

    // --- Capacites offertes (STABLES, jamais renommees) ---------------------
    // Ce que le service SAIT FAIRE, par opposition a ce qu'il S'APPELLE.
    //
    // Le nom est libre : morfSystem etant sous licence GPL, chacun peut renommer
    // une application (« Mon Analyse Meteo », « Weather Lab »...). Un consommateur
    // qui reconnaitrait ses pairs par leur nom casserait donc au premier
    // renommage. Il doit chercher une CAPACITE et n'afficher le nom que comme
    // libelle.
    //
    // Une capacite est un identifiant stable, en minuscules avec tirets bas :
    //   "advanced_analysis"  analyses avancees sur donnees historiques
    //   "notification"       acheminement de notifications
    //   "storage"            stockage/synchronisation de donnees
    //
    // Champ facultatif : laisse vide, il n'est pas emis, et les consommateurs
    // plus anciens ignorent simplement un champ qu'ils ne connaissent pas.
    QStringList capabilities;

    // Capacite reservee : une application exposant une interface Web l'annonce
    // sous ce nom. Utiliser la constante plutot que la chaine, pour que
    // producteur et consommateur ne puissent pas diverger sur l'orthographe.
    static constexpr const char* kCapabilityWebUi = "web_ui";

    // --- Interface Web exposee (facultatif) ---------------------------------
    // Renseigner `webUiPath` DECLARE que l'application expose une interface Web.
    //
    // Repartition volontaire, conforme au principe « push presence / pull
    // detail » : la CAPACITE part dans le heartbeat (diffuse toutes les 15 s par
    // chaque service, donc il doit rester court et stable), le DETAIL ci-dessous
    // n'est publie que par /status, interroge a la demande. Le heartbeat ne
    // devient pas un catalogue de metadonnees.
    //
    // La capacite « web_ui » est AJOUTEE AUTOMATIQUEMENT aux capacites emises
    // des que `webUiPath` est renseigne : declarer le detail sans la capacite
    // rendrait l'interface indecouvrable, et l'inverse produirait un lien mort.
    // Une seule source de verite, donc aucune divergence possible.
    QString webUiPath;          // ex. "/" ; vide => aucune interface declaree
    QString webUiLabel;         // libelle affiche ; defaut : appName
    QString webUiDescription;   // phrase courte, facultative
    quint16 webUiPort = 0;      // 0 => meme port que statusPort (cas courant)

    // --- API metier exposee (facultatif) ------------------------------------
    // Meme repartition que l'interface web : publie UNIQUEMENT par /status,
    // jamais dans le heartbeat. La liste des routes peut etre longue et
    // n'interesse qu'un consommateur qui interroge deja /status (inventaire,
    // cartographie, vue de diagnostic). L'y mettre garde le datagramme
    // periodique minimal.
    //
    // Vide => aucune API declaree, et la cle "api" est absente de /status : un
    // consommateur ne distingue pas « pas d'API » de « API vide ».
    QVector<ApiEndpoint> api;
    QString apiBasePath;        // prefixe commun indicatif, ex. "/api" ; facultatif

    // Heartbeat UDP (presence)
    quint16 udpPort            = 45454;  // port de broadcast (identique pour tout le parc)
    int     broadcastIntervalMs = 15000; // periode d'annonce (15 s)

    // Serveur HTTP local (detail)
    quint16 statusPort        = 8787;                    // 0 => pas de serveur HTTP
    QString statusBindAddress = QStringLiteral("0.0.0.0"); // interfaces ecoutees

    // Version du protocole, incluse dans chaque datagramme. A incrementer si le
    // format du heartbeat change de facon incompatible.
    static constexpr const char* kProto = "morfbeacon/1";
};

// -----------------------------------------------------------------------------
// describeService : serialise le DETAIL annonce d'un service -- interface web et
// liste d'API -- dans le fragment qu'un consommateur lit depuis /status.
//
// Defini UNE seule fois ici pour que les deux producteurs de /status rendent un
// resultat identique :
//   - le StatusServer fourni par la bibliotheque (apps GUI via PresenceService) ;
//   - un service qui sert son PROPRE /status (morfAnalytics, morfMonitor...).
// Sans ce point unique, chaque service reecrit le bloc a la main et derive de la
// bibliotheque comme des autres -- ce que morfAnalytics faisait deja pour web_ui.
//
// Fonction d'en-tete (inline) : tout consommateur qui vendore deja PresenceConfig.h
// en dispose sans fichier source supplementaire a compiler ou a lier.
//
// `statusPort` sert de port par defaut de l'interface web (cas le plus courant :
// l'interface partage le port de /status).
inline QJsonObject describeService(const PresenceConfig& c, quint16 statusPort) {
    QJsonObject o;

    if (!c.webUiPath.isEmpty()) {
        QJsonObject ui;
        ui["path"]  = c.webUiPath;
        ui["label"] = c.webUiLabel.isEmpty() ? c.appName : c.webUiLabel;
        ui["port"]  = static_cast<int>(c.webUiPort != 0 ? c.webUiPort : statusPort);
        if (!c.webUiDescription.isEmpty())
            ui["description"] = c.webUiDescription;
        o["web_ui"] = ui;
    }

    if (!c.api.isEmpty()) {
        QJsonArray endpoints;
        for (const ApiEndpoint& e : c.api) {
            QJsonObject j;
            j["method"] = e.method;
            j["path"]   = e.path;
            if (!e.summary.isEmpty())
                j["summary"] = e.summary;
            endpoints.append(j);
        }
        QJsonObject api;
        if (!c.apiBasePath.isEmpty())
            api["base"] = c.apiBasePath;
        api["endpoints"] = endpoints;
        o["api"] = api;
    }

    return o;
}

} // namespace morfbeacon
