#include "importexport_module.h"
#include <ArduinoJson.h>
#include "../../api/api_router/api_router.h"
#include "../../services/web_assets/web_assets.h"
#include "../../services/config_manager/config_manager.h"
#include "../../services/time_manager/time_manager.h"

using domain::CsvFormat;

namespace {

const char* KEY_LAST_IMPORT = "last_import_at";
const char* KEY_LAST_EXPORT = "last_export_at";
const char* KEY_LAST_BACKUP = "last_backup_at";

CsvFormat formatFromParam(AsyncWebServerRequest* request) {
    if (request->hasParam("format") && request->getParam("format")->value() == "bomist") return CsvFormat::Bomist;
    return CsvFormat::Native;
}

// Horodatage ISO si le WiFi/NTP est synchronisé, sinon repère relatif au
// démarrage (voir InventoryModule pour le même compromis sur les mouvements
// de stock).
String nowIso() {
    return timeMgr.isSynced() ? timeMgr.isoTimestamp() : ("boot+" + String(millis() / 1000) + "s");
}

} // namespace

void ImportExportModule::registerRoutes(WebRouter& router) {
    _registerPages(router);
    _registerApiRoutes(router);
}

void ImportExportModule::_registerPages(WebRouter& router) {
    router.get("/import-export", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/import-export.html"); });
}

void ImportExportModule::_registerApiRoutes(WebRouter& router) {
    router.get("/api/inventory/export", [this](AsyncWebServerRequest* request) {
        CsvFormat format = formatFromParam(request);
        String csv = _service.exportCsv(format).c_str();
        String filename = (format == CsvFormat::Bomist) ? "bomist_export.csv" : "componenthub_export.csv";

        String when = nowIso();
        config.setString(KEY_LAST_EXPORT, when);
        if (format == CsvFormat::Native) config.setString(KEY_LAST_BACKUP, when);

        AsyncWebServerResponse* res = request->beginResponse(200, "text/csv; charset=utf-8", csv);
        res->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        request->send(res);
    });

    router.withBody("/api/inventory/import", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        CsvFormat format = formatFromParam(request);
        domain::ImportResult result = _service.importCsv(std::string(body.c_str()), format);
        config.setString(KEY_LAST_IMPORT, nowIso());

        JsonDocument doc;
        doc["created"] = result.created;
        doc["updated"] = result.updated;
        doc["failed"] = result.failed;
        JsonArray errs = doc["errors"].to<JsonArray>();
        for (auto& e : result.errors) errs.add(e);

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.get("/api/inventory/meta", [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["lastImportAt"] = config.getString(KEY_LAST_IMPORT, "");
        doc["lastExportAt"] = config.getString(KEY_LAST_EXPORT, "");
        doc["lastBackupAt"] = config.getString(KEY_LAST_BACKUP, "");
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });
}
