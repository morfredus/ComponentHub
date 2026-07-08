#include "component_repository.h"
#include "json_store_util.h"
#include <map>

using domain::Component;
using domain::Id;

namespace {

void toJson(JsonObject obj, const Component& c) {
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

Component fromJson(JsonObjectConst obj) {
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

} // namespace

std::vector<Component> ComponentJsonRepository::findAll() const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<Component> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        out.push_back(fromJson(obj));
    }
    return out;
}

std::optional<Component> ComponentJsonRepository::findById(Id id) const {
    for (auto& c : findAll()) {
        if (c.id == id) return c;
    }
    return std::nullopt;
}

Component ComponentJsonRepository::save(Component component) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (component.id == domain::kNoId) {
        component.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), component);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == component.id) {
                toJson(obj, component);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), component);
    }

    JsonStore::save(_path, doc);
    return component;
}

std::vector<Component> ComponentJsonRepository::saveAll(const std::vector<Component>& components) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    // Index des entrées existantes par id : évite un balayage O(n) de tout le
    // tableau pour chaque élément du lot (voir commentaire sur l'interface).
    std::map<Id, JsonObject> byId;
    for (JsonObject obj : items) {
        byId[(Id)(obj["id"] | 0)] = obj;
    }

    std::vector<Component> result;
    result.reserve(components.size());

    for (Component c : components) {
        if (c.id == domain::kNoId) {
            c.id = JsonStore::takeNextId(doc);
            JsonObject obj = items.add<JsonObject>();
            toJson(obj, c);
            byId[c.id] = obj;
        } else {
            auto it = byId.find(c.id);
            if (it != byId.end()) {
                toJson(it->second, c);
            } else {
                JsonObject obj = items.add<JsonObject>();
                toJson(obj, c);
                byId[c.id] = obj;
            }
        }
        result.push_back(c);
    }

    JsonStore::save(_path, doc);
    return result;
}

bool ComponentJsonRepository::remove(Id id) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();
    for (size_t i = 0; i < items.size(); i++) {
        if ((Id)(items[i]["id"] | 0) == id) {
            items.remove(i);
            JsonStore::save(_path, doc);
            return true;
        }
    }
    return false;
}
