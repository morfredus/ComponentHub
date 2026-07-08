#include "document_module.h"
#include <ArduinoJson.h>
#include "../../api/api_router/api_router.h"

using domain::Document;
using domain::DocumentOwnerKind;

namespace {

void documentToJson(JsonObject obj, const Document& d) {
    obj["id"] = d.id;
    obj["ownerKind"] = domain::toString(d.ownerKind);
    obj["ownerId"] = d.ownerId;
    obj["title"] = d.title;
    obj["category"] = d.category;
    obj["url"] = d.url;
    obj["notes"] = d.notes;
}

Document documentFromJson(JsonObjectConst obj) {
    Document d;
    d.id = obj["id"] | 0;
    d.ownerKind = domain::documentOwnerKindFromString(std::string(obj["ownerKind"] | "component"));
    d.ownerId = obj["ownerId"] | 0;
    d.title = std::string(obj["title"] | "");
    d.category = std::string(obj["category"] | "");
    d.url = std::string(obj["url"] | "");
    d.notes = std::string(obj["notes"] | "");
    return d;
}

void sendError(AsyncWebServerRequest* request, int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    request->send(code, "application/json", out);
}

} // namespace

void DocumentModule::registerRoutes(WebRouter& router) {
    router.get("/api/documents", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("ownerKind") || !request->hasParam("ownerId")) {
            sendError(request, 400, "ownerKind et ownerId requis");
            return;
        }
        auto ownerKind = domain::documentOwnerKindFromString(std::string(request->getParam("ownerKind")->value().c_str()));
        int ownerId = request->getParam("ownerId")->value().toInt();

        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& d : _service.listForOwner(ownerKind, ownerId)) documentToJson(items.add<JsonObject>(), d);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/documents", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }

        Document saved = _service.save(documentFromJson(doc.as<JsonObject>()));

        JsonDocument out;
        documentToJson(out.to<JsonObject>(), saved);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/documents", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.remove(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "document introuvable");
    });
}
