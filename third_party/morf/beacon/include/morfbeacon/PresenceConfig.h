/*
 * morfBeacon
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace morfbeacon {

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

} // namespace morfbeacon
