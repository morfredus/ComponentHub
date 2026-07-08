#include "project_component_repository.h"
#include "json_store_util.h"

using domain::ProjectComponent;
using domain::Id;

namespace {

void toJson(JsonObject obj, const ProjectComponent& link) {
    obj["id"] = link.id;
    obj["projectId"] = link.projectId;
    obj["componentId"] = link.componentId;
    obj["label"] = link.label;
    obj["quantityRequired"] = link.quantityRequired;
}

ProjectComponent fromJson(JsonObjectConst obj) {
    ProjectComponent link;
    link.id = obj["id"] | 0;
    link.projectId = obj["projectId"] | 0;
    link.componentId = obj["componentId"] | 0;
    link.label = std::string(obj["label"] | "");
    link.quantityRequired = obj["quantityRequired"] | 1;
    return link;
}

} // namespace

std::vector<ProjectComponent> ProjectComponentJsonRepository::findByProject(Id projectId) const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<ProjectComponent> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        ProjectComponent link = fromJson(obj);
        if (link.projectId == projectId) out.push_back(link);
    }
    return out;
}

ProjectComponent ProjectComponentJsonRepository::save(ProjectComponent link) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (link.id == domain::kNoId) {
        link.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), link);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == link.id) {
                toJson(obj, link);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), link);
    }

    JsonStore::save(_path, doc);
    return link;
}

bool ProjectComponentJsonRepository::remove(Id id) {
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

bool ProjectComponentJsonRepository::removeByProject(Id projectId) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();
    bool removedAny = false;
    for (size_t i = items.size(); i-- > 0;) {
        if ((Id)(items[i]["projectId"] | 0) == projectId) {
            items.remove(i);
            removedAny = true;
        }
    }
    if (removedAny) JsonStore::save(_path, doc);
    return removedAny;
}
