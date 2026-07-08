#include "document_repository.h"
#include "json_store_util.h"

using domain::Document;
using domain::DocumentOwnerKind;
using domain::Id;

namespace {

void toJson(JsonObject obj, const Document& d) {
    obj["id"] = d.id;
    obj["ownerKind"] = domain::toString(d.ownerKind);
    obj["ownerId"] = d.ownerId;
    obj["title"] = d.title;
    obj["category"] = d.category;
    obj["url"] = d.url;
    obj["notes"] = d.notes;
}

Document fromJson(JsonObjectConst obj) {
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

} // namespace

std::vector<Document> DocumentJsonRepository::findByOwner(DocumentOwnerKind ownerKind, Id ownerId) const {
    JsonDocument doc = JsonStore::load(_path);
    std::vector<Document> out;
    for (JsonObject obj : doc["items"].as<JsonArray>()) {
        Document d = fromJson(obj);
        if (d.ownerKind == ownerKind && d.ownerId == ownerId) out.push_back(d);
    }
    return out;
}

Document DocumentJsonRepository::save(Document document) {
    JsonDocument doc = JsonStore::load(_path);
    JsonArray items = doc["items"].as<JsonArray>();

    if (document.id == domain::kNoId) {
        document.id = JsonStore::takeNextId(doc);
        toJson(items.add<JsonObject>(), document);
    } else {
        bool found = false;
        for (JsonObject obj : items) {
            if ((Id)(obj["id"] | 0) == document.id) {
                toJson(obj, document);
                found = true;
                break;
            }
        }
        if (!found) toJson(items.add<JsonObject>(), document);
    }

    JsonStore::save(_path, doc);
    return document;
}

bool DocumentJsonRepository::remove(Id id) {
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
