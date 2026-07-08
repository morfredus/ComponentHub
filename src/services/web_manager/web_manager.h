/**
 * WebManager — Serveur HTTP asynchrone du framework (ESPAsyncWebServer).
 *
 * Fournit automatiquement, sans code métier :
 *   GET  /            — page d'accueil (asset embarqué index.html)
 *   GET  /logs        — page de consultation des logs
 *   GET  /api/logs     — tampon de logs en JSON
 *   GET  /files        — gestionnaire de fichiers LittleFS (documents utilisateur)
 *   GET  /api/files/list|download, POST /api/files/upload, DELETE /api/files/delete
 *   GET  /system       — informations système
 *   GET  /api/system    — JSON (SystemInfo::getJson)
 *   GET  /ota           — page de mise à jour firmware
 *   POST /api/ota/update — délègue à OtaManager
 *
 * Tout asset embarqué (voir src/services/web_assets/) non repris ci-dessus
 * est servi tel quel par le handler générique (onNotFound) — l'interface
 * web vit en PROGMEM dans le firmware, plus sur LittleFS (voir
 * docs/ARCHITECTURE.md). Les modules métier ajoutent leurs propres routes
 * via WebRouter (registerRoutes), sans connaître l'implémentation de
 * WebManager.
 *
 * Étant asynchrone, ce serveur ne nécessite pas d'appel de type
 * handleClient() dans loop() — loop() est conservée (no-op) uniquement pour
 * ne pas devoir modifier App::loop() (core/, non touché par ce changement).
 */

#pragma once
#include <ESPAsyncWebServer.h>
#include "../../api/api_router/api_router.h"

class WebManager {
public:
    void begin(uint16_t port = 80);
    void loop() {}

    WebRouter router();

private:
    void _registerCoreRoutes();

    AsyncWebServer _server{80};
    bool _started = false;
};

extern WebManager webMgr;
