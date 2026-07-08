#include "project_module.h"
#include <ArduinoJson.h>
#include "../../api/api_router/api_router.h"
#include "../../services/web_assets/web_assets.h"

using domain::Project;
using domain::ProjectComponentView;
using domain::Id;

namespace {

void projectToJson(JsonObject obj, const Project& p) {
    obj["id"] = p.id;
    obj["name"] = p.name;
    obj["description"] = p.description;
    obj["version"] = p.version;
    obj["firmware"] = p.firmware;
    obj["gitRepo"] = p.gitRepo;
    obj["status"] = p.status;
    obj["notes"] = p.notes;
}

Project projectFromJson(JsonObjectConst obj) {
    Project p;
    p.id = obj["id"] | 0;
    p.name = std::string(obj["name"] | "");
    p.description = std::string(obj["description"] | "");
    p.version = std::string(obj["version"] | "");
    p.firmware = std::string(obj["firmware"] | "");
    p.gitRepo = std::string(obj["gitRepo"] | "");
    p.status = std::string(obj["status"] | "");
    p.notes = std::string(obj["notes"] | "");
    return p;
}

void bomViewToJson(JsonObject obj, const ProjectComponentView& v) {
    obj["id"] = v.id;
    obj["componentId"] = v.componentId;
    obj["componentName"] = v.componentName;
    obj["reference"] = v.reference;
    obj["inInventory"] = v.inInventory;
    obj["unitPrice"] = v.unitPrice;
    obj["quantityRequired"] = v.quantityRequired;
    obj["quantityAvailable"] = v.quantityAvailable;
    obj["quantityMissing"] = v.quantityMissing;
}

void sendError(AsyncWebServerRequest* request, int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    request->send(code, "application/json", out);
}

} // namespace

void ProjectModule::registerRoutes(WebRouter& router) {
    _registerPages(router);
    // ORDRE IMPORTANT : les routes spécifiques (/api/projects/bom, /missing,
    // /component) doivent être enregistrées AVANT la route de liste générique
    // /api/projects. ESPAsyncWebServer teste les handlers dans l'ordre
    // d'enregistrement et fait correspondre par préfixe : un handler sur
    // "/api/projects" capterait sinon aussi "/api/projects/bom"
    // (url.startsWith("/api/projects/")), renvoyant la liste des projets à la
    // place de la nomenclature.
    _registerBomRoutes(router);
    _registerProjectRoutes(router);
}

void ProjectModule::_registerPages(WebRouter& router) {
    router.get("/projects", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/projects.html"); });
}

void ProjectModule::_registerProjectRoutes(WebRouter& router) {
    router.get("/api/projects", [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& p : _service.listProjects()) projectToJson(items.add<JsonObject>(), p);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.get("/api/project", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        auto project = _service.getProject(request->getParam("id")->value().toInt());
        if (!project) { sendError(request, 404, "projet introuvable"); return; }

        JsonDocument doc;
        projectToJson(doc.to<JsonObject>(), *project);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/project", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }

        Project saved = _service.saveProject(projectFromJson(doc.as<JsonObject>()));

        JsonDocument out;
        projectToJson(out.to<JsonObject>(), saved);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/project", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.removeProject(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "projet introuvable");
    });
}

void ProjectModule::_registerBomRoutes(WebRouter& router) {
    router.get("/api/projects/bom", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("projectId")) { sendError(request, 400, "projectId requis"); return; }
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& v : _service.bom(request->getParam("projectId")->value().toInt()))
            bomViewToJson(items.add<JsonObject>(), v);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.get("/api/projects/missing", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("projectId")) { sendError(request, 400, "projectId requis"); return; }
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& v : _service.missingComponents(request->getParam("projectId")->value().toInt()))
            bomViewToJson(items.add<JsonObject>(), v);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/projects/component", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }
        JsonObject obj = doc.as<JsonObject>();

        auto link = _service.addComponent(obj["projectId"] | 0, obj["componentId"] | 0,
                                          std::string(obj["label"] | ""), obj["quantityRequired"] | 1);

        JsonDocument out;
        out["id"] = link.id;
        out["projectId"] = link.projectId;
        out["componentId"] = link.componentId;
        out["label"] = link.label;
        out["quantityRequired"] = link.quantityRequired;
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/projects/component", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.removeComponent(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "ligne de nomenclature introuvable");
    });
}
