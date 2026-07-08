#include "web_manager.h"
#include "../ota_manager/ota_manager.h"
#include "../system_info/system_info.h"
#include "../log_manager/log_manager.h"
#include "../storage_manager/storage_manager.h"
#include "../web_assets/web_assets.h"
#include "project_config.h"
#include <LittleFS.h>

static const char* TAG = "Web";

WebManager webMgr;

// Content-Type pour les fichiers utilisateur (documents uploadés via
// /api/files/*, sur LittleFS) — distinct de la table WebAssets (interface
// web embarquée), dont le Content-Type est déjà connu à la compilation.
static String _contentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    return "text/plain";
}

WebRouter WebManager::router() { return WebRouter(_server); }

void WebManager::begin(uint16_t port) {
    if (_started) return;
    _started = true;

    _registerCoreRoutes();
    WebRouter r = router();
    otaMgr.registerRoutes(r);
    storage.begin();

    // Repli générique : tout asset embarqué (style.css, menu.js, inventory.js...)
    // dont le chemin exact n'a pas de route explicite ci-dessous.
    _server.onNotFound([](AsyncWebServerRequest* request) {
        String path = request->url();
        if (path.indexOf("..") != -1) { request->send(403, "text/plain", "Forbidden"); return; }
        // WebAssets::send ajoute Cache-Control: no-cache et gère le 404.
        if (WebAssets::find(path)) { WebAssets::send(request, path.c_str()); return; }
        request->send(404, "text/plain", "Not Found");
    });

    _server.begin();
    LOG_INFO(TAG, "Serveur web démarré sur le port %u", (unsigned)port);
}

void WebManager::_registerCoreRoutes() {
    WebRouter r = router();

    // --- Page d'accueil et pages statiques connues, embarquées en PROGMEM ---
    r.get("/", [](AsyncWebServerRequest* request) {
        if (WebAssets::find("/index.html")) WebAssets::send(request, "/index.html");
        else request->send(200, "text/html", String("<h1>") + PROJECT_NAME + "</h1><p>Aucune page d'accueil (web/index.html absent — régénérer avec tools/generate_web_assets.py).</p>");
    });

    r.get("/logs", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/logs.html"); });
    r.get("/files", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/files.html"); });
    r.get("/system", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/system.html"); });
    r.get("/ota", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/ota.html"); });

    // --- API logs ---
    r.get("/api/logs", [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", logMgr.toJson());
        res->addHeader("Cache-Control", "no-cache");
        request->send(res);
    });
    r.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        logMgr.clear();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // --- API système ---
    r.get("/api/system", [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", systemInfo.getJson());
        res->addHeader("Cache-Control", "no-cache");
        request->send(res);
    });

    // --- API fichiers (LittleFS) ---
    r.get("/api/files/list", [](AsyncWebServerRequest* request) {
        String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
        auto files = storage.listFiles(path);
        String json = "[";
        for (size_t i = 0; i < files.size(); i++) {
            if (i) json += ",";
            json += "{\"name\":\"" + files[i].name + "\",\"size\":" + files[i].size +
                     ",\"isDir\":" + (files[i].isDir ? "true" : "false") + "}";
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    r.get("/api/files/download", [](AsyncWebServerRequest* request) {
        if (!request->hasParam("path")) { request->send(400, "text/plain", "path requis"); return; }
        String path = request->getParam("path")->value();
        if (path.indexOf("..") != -1 || !storage.exists(path)) {
            request->send(404, "text/plain", "Fichier introuvable");
            return;
        }
        request->send(LittleFS, path, _contentType(path));
    });

    r.on("/api/files/delete", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("path")) { request->send(400, "application/json", "{\"error\":\"path requis\"}"); return; }
        String path = request->getParam("path")->value();
        if (path.isEmpty() || path == "/" || path.indexOf("..") != -1) {
            request->send(403, "application/json", "{\"error\":\"chemin invalide\"}");
            return;
        }
        bool ok = storage.deleteFile(path);
        request->send(ok ? 200 : 404, "application/json", ok ? "{\"status\":\"ok\"}" : "{\"error\":\"introuvable\"}");
    });

    r.withUpload("/api/files/upload", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"status\":\"ok\"}"); },
        [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool isFinal) {
            static File uploadFile;
            if (index == 0) {
                String path = filename.startsWith("/") ? filename : "/" + filename;
                if (path.indexOf("..") != -1) return;
                uploadFile = LittleFS.open(path, "w");
            }
            if (uploadFile) uploadFile.write(data, len);
            if (isFinal && uploadFile) uploadFile.close();
        });
}
