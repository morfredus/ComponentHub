#include "web_assets.h"

const WebAsset* WebAssets::find(const String& path) {
    for (size_t i = 0; i < kWebAssetCount; i++) {
        if (path.equals(kWebAssets[i].path)) return &kWebAssets[i];
    }
    return nullptr;
}

void WebAssets::send(AsyncWebServerRequest* request, const char* path, int code) {
    const WebAsset* asset = find(path);
    if (!asset) {
        request->send(404, "text/plain",
                       String("Asset embarqué introuvable : ") + path +
                       " (régénérer avec tools/generate_web_assets.py après un changement de web_src/)");
        return;
    }
    // Cache-Control: no-cache — les assets ont une URL stable (/inventory.js…)
    // mais un contenu qui change à chaque firmware. Sans revalidation, le
    // navigateur peut servir un ancien HTML/JS après une mise à jour, d'où des
    // incohérences (ex. JS attendant un élément que le nouveau HTML n'a plus).
    // "no-cache" force une revalidation à chaque chargement ; sur réseau local
    // le surcoût est négligeable.
    AsyncWebServerResponse* res = request->beginResponse(code, asset->contentType, asset->data, asset->length);
    res->addHeader("Cache-Control", "no-cache");
    request->send(res);
}
