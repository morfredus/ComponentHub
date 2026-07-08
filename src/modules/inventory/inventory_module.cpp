#include "inventory_module.h"
#include <ArduinoJson.h>
#include "../../api/api_router/api_router.h"
#include "../../services/log_manager/log_manager.h"
#include "../../services/web_assets/web_assets.h"
#include "../../services/time_manager/time_manager.h"

using domain::Component;
using domain::Location;
using domain::Category;
using domain::StockMovement;
using domain::Id;

namespace {

void componentToJson(JsonObject obj, const Component& c) {
    obj["id"] = c.id;
    obj["kind"] = domain::toString(c.kind);
    obj["name"] = c.name;
    obj["reference"] = c.reference;
    obj["manufacturer"] = c.manufacturer;
    obj["description"] = c.description;
    obj["category"] = c.category;
    obj["subcategory"] = c.subcategory;
    obj["type"] = c.type;
    obj["voltage"] = c.voltage;
    obj["current"] = c.current;
    obj["interfaceType"] = c.interfaceType;
    obj["protocols"] = c.protocols;
    obj["i2cAddress"] = c.i2cAddress;
    obj["frequency"] = c.frequency;
    obj["pinCount"] = c.pinCount;
    obj["compatibility"] = c.compatibility;
    obj["price"] = c.price;
    obj["supplier"] = c.supplier;
    obj["purchaseDate"] = c.purchaseDate;
    obj["receptionDate"] = c.receptionDate;
    obj["warranty"] = c.warranty;
    obj["state"] = c.state;
    obj["status"] = c.status;
    obj["origin"] = c.origin;
    obj["notes"] = c.notes;
    obj["locationId"] = c.locationId;
    obj["quantity"] = c.quantity;
    obj["minStock"] = c.minStock;
    obj["idealStock"] = c.idealStock;
}

Component componentFromJson(JsonObjectConst obj) {
    Component c;
    c.id = obj["id"] | 0;
    c.kind = domain::componentKindFromString(std::string(obj["kind"] | "component"));
    c.name = std::string(obj["name"] | "");
    c.reference = std::string(obj["reference"] | "");
    c.manufacturer = std::string(obj["manufacturer"] | "");
    c.description = std::string(obj["description"] | "");
    c.category = std::string(obj["category"] | "");
    c.subcategory = std::string(obj["subcategory"] | "");
    c.type = std::string(obj["type"] | "");
    c.voltage = std::string(obj["voltage"] | "");
    c.current = std::string(obj["current"] | "");
    c.interfaceType = std::string(obj["interfaceType"] | "");
    c.protocols = std::string(obj["protocols"] | "");
    c.i2cAddress = std::string(obj["i2cAddress"] | "");
    c.frequency = std::string(obj["frequency"] | "");
    c.pinCount = obj["pinCount"] | 0;
    c.compatibility = std::string(obj["compatibility"] | "");
    c.price = obj["price"] | 0.0;
    c.supplier = std::string(obj["supplier"] | "");
    c.purchaseDate = std::string(obj["purchaseDate"] | "");
    c.receptionDate = std::string(obj["receptionDate"] | "");
    c.warranty = std::string(obj["warranty"] | "");
    c.state = std::string(obj["state"] | "");
    c.status = std::string(obj["status"] | "to_test");
    c.origin = std::string(obj["origin"] | "");
    c.notes = std::string(obj["notes"] | "");
    c.locationId = obj["locationId"] | 0;
    c.quantity = obj["quantity"] | 0;
    c.minStock = obj["minStock"] | 0;
    c.idealStock = obj["idealStock"] | 0;
    return c;
}

void locationToJson(JsonObject obj, const Location& l) {
    obj["id"] = l.id;
    obj["name"] = l.name;
    obj["parentId"] = l.parentId;
}

Location locationFromJson(JsonObjectConst obj) {
    Location l;
    l.id = obj["id"] | 0;
    l.name = std::string(obj["name"] | "");
    l.parentId = obj["parentId"] | 0;
    return l;
}

void categoryToJson(JsonObject obj, const Category& c) {
    obj["id"] = c.id;
    obj["name"] = c.name;
}

Category categoryFromJson(JsonObjectConst obj) {
    Category c;
    c.id = obj["id"] | 0;
    c.name = std::string(obj["name"] | "");
    return c;
}

void movementToJson(JsonObject obj, const StockMovement& m) {
    obj["id"] = m.id;
    obj["componentId"] = m.componentId;
    obj["type"] = domain::toString(m.type);
    obj["quantity"] = m.quantity;
    obj["timestamp"] = m.timestamp;
    obj["note"] = m.note;
}

void sendError(AsyncWebServerRequest* request, int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    request->send(code, "application/json", out);
}

} // namespace

void InventoryModule::begin() {
    LOG_INFO(name(), "Démarré — %u composant(s), %u emplacement(s)",
             (unsigned)_service.listComponents().size(),
             (unsigned)_service.listLocations().size());
}

void InventoryModule::registerRoutes(WebRouter& router) {
    _registerPages(router);
    _registerComponentRoutes(router);
    _registerLocationRoutes(router);
    _registerCategoryRoutes(router);
    _registerStockRoutes(router);
}

void InventoryModule::_registerPages(WebRouter& router) {
    router.get("/inventory", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/inventory.html"); });
    router.get("/locations", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/locations.html"); });
    router.get("/categories", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/categories.html"); });
    // Page de navigation (cartouches vers Emplacements/Catégories/Import-Export) —
    // pas de contenu propre à l'inventaire, mais aucun module ne "possède" ce
    // regroupement plus légitimement qu'un autre.
    router.get("/settings", [](AsyncWebServerRequest* request) { WebAssets::send(request, "/settings.html"); });
}

void InventoryModule::_registerComponentRoutes(WebRouter& router) {
    router.get("/api/inventory/components", [this](AsyncWebServerRequest* request) {
        domain::ComponentFilter filter;
        if (request->hasParam("q"))          filter.query = request->getParam("q")->value().c_str();
        if (request->hasParam("category"))   filter.category = request->getParam("category")->value().c_str();
        if (request->hasParam("status"))     filter.status = request->getParam("status")->value().c_str();
        if (request->hasParam("locationId")) filter.locationId = request->getParam("locationId")->value().toInt();
        if (request->hasParam("lowStock"))   filter.onlyLowStock = request->getParam("lowStock")->value() == "1";
        if (request->hasParam("kind")) {
            filter.hasKind = true;
            filter.kind = domain::componentKindFromString(std::string(request->getParam("kind")->value().c_str()));
        }

        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& c : _service.listComponents(filter)) componentToJson(items.add<JsonObject>(), c);

        String out;
        serializeJson(doc, out);
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", out);
        res->addHeader("Cache-Control", "no-cache");
        request->send(res);
    });

    router.get("/api/inventory/component", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        auto comp = _service.getComponent(request->getParam("id")->value().toInt());
        if (!comp) { sendError(request, 404, "composant introuvable"); return; }

        JsonDocument doc;
        componentToJson(doc.to<JsonObject>(), *comp);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/inventory/component", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }

        Component saved = _service.saveComponent(componentFromJson(doc.as<JsonObject>()));

        JsonDocument out;
        componentToJson(out.to<JsonObject>(), saved);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/inventory/component", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.removeComponent(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "composant introuvable");
    });

    router.get("/api/inventory/low-stock", [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& c : _service.lowStock()) componentToJson(items.add<JsonObject>(), c);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });
}

void InventoryModule::_registerLocationRoutes(WebRouter& router) {
    router.get("/api/inventory/locations", [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& l : _service.listLocations()) locationToJson(items.add<JsonObject>(), l);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/inventory/location", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }

        Location saved = _service.saveLocation(locationFromJson(doc.as<JsonObject>()));

        JsonDocument out;
        locationToJson(out.to<JsonObject>(), saved);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/inventory/location", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.removeLocation(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "emplacement introuvable");
    });
}

void InventoryModule::_registerCategoryRoutes(WebRouter& router) {
    router.get("/api/inventory/categories", [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& c : _service.listCategories()) categoryToJson(items.add<JsonObject>(), c);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/inventory/category", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }

        Category saved = _service.saveCategory(categoryFromJson(doc.as<JsonObject>()));

        JsonDocument out;
        categoryToJson(out.to<JsonObject>(), saved);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });

    router.on("/api/inventory/category", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) { sendError(request, 400, "id requis"); return; }
        bool ok = _service.removeCategory(request->getParam("id")->value().toInt());
        if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}");
        else sendError(request, 404, "catégorie introuvable");
    });
}

void InventoryModule::_registerStockRoutes(WebRouter& router) {
    router.get("/api/inventory/stock/history", [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("componentId")) { sendError(request, 400, "componentId requis"); return; }

        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (auto& m : _service.history(request->getParam("componentId")->value().toInt()))
            movementToJson(items.add<JsonObject>(), m);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    router.withBody("/api/inventory/stock/movement", HTTP_POST, [this](AsyncWebServerRequest* request, const String& body) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { sendError(request, 400, "JSON invalide"); return; }
        JsonObject obj = doc.as<JsonObject>();

        StockMovement m;
        m.componentId = obj["componentId"] | 0;
        m.type = domain::stockMovementTypeFromString(std::string(obj["type"] | "in"));
        m.quantity = obj["quantity"] | 0;
        m.note = std::string(obj["note"] | "");
        m.timestamp = std::string(
            (timeMgr.isSynced() ? timeMgr.isoTimestamp() : ("boot+" + String(millis() / 1000) + "s")).c_str());

        auto updated = _service.recordMovement(m);
        if (!updated) { sendError(request, 404, "composant introuvable"); return; }

        JsonDocument out;
        componentToJson(out.to<JsonObject>(), *updated);
        String outStr;
        serializeJson(out, outStr);
        request->send(200, "application/json", outStr);
    });
}
