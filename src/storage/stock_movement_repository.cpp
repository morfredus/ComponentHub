#include "stock_movement_repository.h"
#include "json_store_util.h"

using domain::StockMovement;
using domain::Id;

namespace {

void toJson(JsonObject obj, const StockMovement& m) {
    obj["id"] = m.id;
    obj["componentId"] = m.componentId;
    obj["type"] = domain::toString(m.type);
    obj["quantity"] = m.quantity;
    obj["timestamp"] = m.timestamp;
    obj["note"] = m.note;
}

StockMovement fromJson(JsonObjectConst obj) {
    StockMovement m;
    m.id = obj["id"] | 0;
    m.componentId = obj["componentId"] | 0;
    m.type = domain::stockMovementTypeFromString(std::string(obj["type"] | "in"));
    m.quantity = obj["quantity"] | 0;
    m.timestamp = std::string(obj["timestamp"] | "");
    m.note = std::string(obj["note"] | "");
    return m;
}

} // namespace

std::vector<StockMovement> StockMovementJsonRepository::findByComponent(Id componentId) const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<StockMovement> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        StockMovement m = fromJson(obj);
        if (m.componentId == componentId) out.push_back(m);
    }
    return out;
}

StockMovement StockMovementJsonRepository::add(StockMovement movement) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();
    movement.id = JsonStore::takeNextId(doc);
    toJson(items.add<JsonObject>(), movement);
    JsonStore::save(_path, doc);
    return movement;
}
