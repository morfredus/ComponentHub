/**
 * WebAssets — Interface web embarquée dans le firmware (PROGMEM), plutôt que
 * servie depuis LittleFS.
 *
 * `kWebAssets`/`kWebAssetCount` sont générés par tools/generate_web_assets.py
 * (voir src/generated/web_assets_data.cpp, régénéré à chaque build — ne
 * jamais éditer ce fichier à la main) à partir de web/ (lui-même minifié
 * depuis web_src/ par tools/minify_web.py).
 *
 * Conséquence recherchée : un `pio run --target upload` met à jour le
 * firmware ET l'interface web en un seul geste, et LittleFS ne contient plus
 * que les données réelles de l'inventaire (composants, emplacements,
 * documents utilisateur...) — jamais touché par une mise à jour firmware/OTA.
 */

#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

struct WebAsset {
    const char* path;         // chemin exact tel que demandé, ex: "/index.html"
    const uint8_t* data;
    size_t length;
    const char* contentType;
};

extern const WebAsset kWebAssets[];
extern const size_t kWebAssetCount;

namespace WebAssets {

const WebAsset* find(const String& path);

// Envoie l'asset correspondant à `path`, ou une réponse 404 explicite s'il
// est absent de la table générée (build à régénérer après un changement de
// web_src/ non répercuté).
void send(AsyncWebServerRequest* request, const char* path, int code = 200);

} // namespace WebAssets
