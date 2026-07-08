#include "category_repository.h"
#include "json_store_util.h"

using domain::Category;
using domain::Id;

namespace {

void toJson(JsonObject obj, const Category& c) {
    obj["id"] = c.id;
    obj["name"] = c.name;
}

Category fromJson(JsonObjectConst obj) {
    Category c;
    c.id = obj["id"] | 0;
    c.name = std::string(obj["name"] | "");
    return c;
}

} // namespace

std::vector<Category> CategoryJsonRepository::findAll() const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<Category> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        out.push_back(fromJson(obj));
    }
    return out;
}

std::optional<Category> CategoryJsonRepository::findById(Id id) const {
    for (auto& c : findAll()) {
        if (c.id == id) return c;
    }
    return std::nullopt;
}

Category CategoryJsonRepository::save(Category category) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (category.id == domain::kNoId) {
        category.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), category);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == category.id) {
                toJson(obj, category);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), category);
    }

    JsonStore::save(_path, doc);
    return category;
}

bool CategoryJsonRepository::remove(Id id) {
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
