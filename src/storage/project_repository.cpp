#include "project_repository.h"
#include "json_store_util.h"

using domain::Project;
using domain::Id;

namespace {

void toJson(JsonObject obj, const Project& p) {
    obj["id"] = p.id;
    obj["name"] = p.name;
    obj["description"] = p.description;
    obj["version"] = p.version;
    obj["firmware"] = p.firmware;
    obj["gitRepo"] = p.gitRepo;
    obj["status"] = p.status;
    obj["notes"] = p.notes;
}

Project fromJson(JsonObjectConst obj) {
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

} // namespace

std::vector<Project> ProjectJsonRepository::findAll() const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<Project> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        out.push_back(fromJson(obj));
    }
    return out;
}

std::optional<Project> ProjectJsonRepository::findById(Id id) const {
    for (auto& p : findAll()) {
        if (p.id == id) return p;
    }
    return std::nullopt;
}

Project ProjectJsonRepository::save(Project project) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (project.id == domain::kNoId) {
        project.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), project);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == project.id) {
                toJson(obj, project);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), project);
    }

    JsonStore::save(_path, doc);
    return project;
}

bool ProjectJsonRepository::remove(Id id) {
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
