#include "location_repository.h"
#include "json_store_util.h"

using domain::Location;
using domain::Id;

namespace {

void toJson(JsonObject obj, const Location& l) {
    obj["id"] = l.id;
    obj["name"] = l.name;
    obj["parentId"] = l.parentId;
}

Location fromJson(JsonObjectConst obj) {
    Location l;
    l.id = obj["id"] | 0;
    l.name = std::string(obj["name"] | "");
    l.parentId = obj["parentId"] | 0;
    return l;
}

} // namespace

std::vector<Location> LocationJsonRepository::findAll() const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<Location> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        out.push_back(fromJson(obj));
    }
    return out;
}

std::optional<Location> LocationJsonRepository::findById(Id id) const {
    for (auto& l : findAll()) {
        if (l.id == id) return l;
    }
    return std::nullopt;
}

Location LocationJsonRepository::save(Location location) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (location.id == domain::kNoId) {
        location.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), location);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == location.id) {
                toJson(obj, location);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), location);
    }

    JsonStore::save(_path, doc);
    return location;
}

bool LocationJsonRepository::remove(Id id) {
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
