/**
 * OtaManager — Mise à jour du firmware Over-The-Air (générique, réutilisable)
 *
 * Deux méthodes :
 *   1. ArduinoOTA (réseau) — pio run --target upload --upload-port <host>.local
 *   2. Web OTA (navigateur) — POST /api/ota/update (formulaire upload sur /ota)
 *
 * Activé/désactivé via #define ENABLE_OTA dans project_config.h
 */

#pragma once
#include "../../api/api_router/api_router.h"

class OtaManager {
public:
    // Démarre ArduinoOTA. Idempotent pour les callbacks ; à rappeler à
    // chaque reconnexion WiFi pour ré-ouvrir le port UDP.
    void begin(const char* hostname);

    // Traitement des paquets ArduinoOTA — appeler dans loop().
    void loop();

    // Enregistre la route HTTP POST /api/ota/update (upload .bin).
    void registerRoutes(WebRouter& router);

private:
    bool _callbacksRegistered = false;
};

extern OtaManager otaMgr;
