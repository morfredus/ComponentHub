#include "FileRepositories.h"
#include "JsonStore.h"

#include <algorithm>
#include <unordered_map>

using namespace domain;
using chstore::json;

namespace {

// --- Component <-> JSON (mêmes clés que src/storage/component_repository.cpp) ---
json toJson(const Component& c) {
    return {
        {"id", c.id}, {"kind", toString(c.kind)}, {"name", c.name}, {"reference", c.reference},
        {"manufacturer", c.manufacturer}, {"description", c.description}, {"category", c.category},
        {"subcategory", c.subcategory}, {"type", c.type}, {"voltage", c.voltage}, {"current", c.current},
        {"interfaceType", c.interfaceType}, {"protocols", c.protocols}, {"i2cAddress", c.i2cAddress},
        {"frequency", c.frequency}, {"pinCount", c.pinCount}, {"compatibility", c.compatibility},
        {"price", c.price}, {"supplier", c.supplier}, {"purchaseDate", c.purchaseDate},
        {"receptionDate", c.receptionDate}, {"warranty", c.warranty}, {"state", c.state},
        {"status", c.status}, {"origin", c.origin}, {"notes", c.notes}, {"locationId", c.locationId},
        {"quantity", c.quantity}, {"minStock", c.minStock}, {"idealStock", c.idealStock},
    };
}

// Lecture tolérante : chaque champ a un défaut si absent ou d'un autre type.
std::string str(const json& o, const char* k, const char* def = "") {
    auto it = o.find(k);
    return (it != o.end() && it->is_string()) ? it->get<std::string>() : std::string(def);
}
int integer(const json& o, const char* k, int def = 0) {
    auto it = o.find(k);
    return (it != o.end() && it->is_number()) ? it->get<int>() : def;
}
double number(const json& o, const char* k, double def = 0.0) {
    auto it = o.find(k);
    return (it != o.end() && it->is_number()) ? it->get<double>() : def;
}

Component componentFrom(const json& o) {
    Component c;
    c.id = integer(o, "id");
    c.kind = componentKindFromString(str(o, "kind", "component"));
    c.name = str(o, "name");
    c.reference = str(o, "reference");
    c.manufacturer = str(o, "manufacturer");
    c.description = str(o, "description");
    c.category = str(o, "category");
    c.subcategory = str(o, "subcategory");
    c.type = str(o, "type");
    c.voltage = str(o, "voltage");
    c.current = str(o, "current");
    c.interfaceType = str(o, "interfaceType");
    c.protocols = str(o, "protocols");
    c.i2cAddress = str(o, "i2cAddress");
    c.frequency = str(o, "frequency");
    c.pinCount = integer(o, "pinCount");
    c.compatibility = str(o, "compatibility");
    c.price = number(o, "price");
    c.supplier = str(o, "supplier");
    c.purchaseDate = str(o, "purchaseDate");
    c.receptionDate = str(o, "receptionDate");
    c.warranty = str(o, "warranty");
    c.state = str(o, "state");
    c.status = str(o, "status", "to_test");
    c.origin = str(o, "origin");
    c.notes = str(o, "notes");
    c.locationId = integer(o, "locationId");
    c.quantity = integer(o, "quantity");
    c.minStock = integer(o, "minStock");
    c.idealStock = integer(o, "idealStock");
    return c;
}

// Trouve l'objet d'id donné dans le tableau "items" ; nullptr si absent.
json* findItem(json& items, Id id) {
    for (auto& it : items) if (integer(it, "id") == id) return &it;
    return nullptr;
}

} // namespace

// ============================ ComponentRepository ============================
std::vector<Component> chdesktop::ComponentRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Component> out;
    for (auto& o : doc["items"]) out.push_back(componentFrom(o));
    return out;
}
std::optional<Component> chdesktop::ComponentRepository::findById(Id id) const {
    for (auto& c : findAll()) if (c.id == id) return c;
    return std::nullopt;
}
Component chdesktop::ComponentRepository::save(Component c) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    if (c.id == kNoId) {
        c.id = chstore::takeNextId(doc);
        items.push_back(toJson(c));
    } else if (json* found = findItem(items, c.id)) {
        *found = toJson(c);
    } else {
        items.push_back(toJson(c));
    }
    chstore::save(_path, doc);
    return c;
}
std::vector<Component> chdesktop::ComponentRepository::saveAll(const std::vector<Component>& cs) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    std::unordered_map<Id, size_t> index;
    for (size_t i = 0; i < items.size(); ++i) index[integer(items[i], "id")] = i;

    std::vector<Component> result;
    result.reserve(cs.size());
    for (Component c : cs) {
        if (c.id == kNoId) {
            c.id = chstore::takeNextId(doc);
            index[c.id] = items.size();
            items.push_back(toJson(c));
        } else {
            auto it = index.find(c.id);
            if (it != index.end()) items[it->second] = toJson(c);
            else { index[c.id] = items.size(); items.push_back(toJson(c)); }
        }
        result.push_back(c);
    }
    chstore::save(_path, doc);
    return result;
}
bool chdesktop::ComponentRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    }
    return false;
}

// ============================ LocationRepository =============================
std::vector<Location> chdesktop::LocationRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Location> out;
    for (auto& o : doc["items"]) {
        Location l;
        l.id = integer(o, "id");
        l.name = str(o, "name");
        l.parentId = integer(o, "parentId");
        out.push_back(l);
    }
    return out;
}
std::optional<Location> chdesktop::LocationRepository::findById(Id id) const {
    for (auto& l : findAll()) if (l.id == id) return l;
    return std::nullopt;
}
Location chdesktop::LocationRepository::save(Location l) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    json obj = {{"id", l.id}, {"name", l.name}, {"parentId", l.parentId}};
    if (l.id == kNoId) {
        l.id = chstore::takeNextId(doc);
        obj["id"] = l.id;
        items.push_back(obj);
    } else if (json* found = findItem(items, l.id)) {
        *found = obj;
    } else {
        items.push_back(obj);
    }
    chstore::save(_path, doc);
    return l;
}
bool chdesktop::LocationRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}

// ============================ CategoryRepository =============================
std::vector<Category> chdesktop::CategoryRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Category> out;
    for (auto& o : doc["items"]) { Category c; c.id = integer(o, "id"); c.name = str(o, "name"); out.push_back(c); }
    return out;
}
std::optional<Category> chdesktop::CategoryRepository::findById(Id id) const {
    for (auto& c : findAll()) if (c.id == id) return c;
    return std::nullopt;
}
Category chdesktop::CategoryRepository::save(Category c) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    json obj = {{"id", c.id}, {"name", c.name}};
    if (c.id == kNoId) { c.id = chstore::takeNextId(doc); obj["id"] = c.id; items.push_back(obj); }
    else if (json* found = findItem(items, c.id)) { *found = obj; }
    else { items.push_back(obj); }
    chstore::save(_path, doc);
    return c;
}
bool chdesktop::CategoryRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}

// ========================= StockMovementRepository ==========================
std::vector<StockMovement> chdesktop::StockMovementRepository::findByComponent(Id componentId) const {
    json doc = chstore::load(_path);
    std::vector<StockMovement> out;
    for (auto& o : doc["items"]) {
        if (integer(o, "componentId") != componentId) continue;
        StockMovement m;
        m.id = integer(o, "id");
        m.componentId = integer(o, "componentId");
        m.type = stockMovementTypeFromString(str(o, "type", "in"));
        m.quantity = integer(o, "quantity");
        m.timestamp = str(o, "timestamp");
        m.note = str(o, "note");
        out.push_back(m);
    }
    return out;
}
StockMovement chdesktop::StockMovementRepository::add(StockMovement m) {
    json doc = chstore::load(_path);
    m.id = chstore::takeNextId(doc);
    doc["items"].push_back({
        {"id", m.id}, {"componentId", m.componentId}, {"type", toString(m.type)},
        {"quantity", m.quantity}, {"timestamp", m.timestamp}, {"note", m.note},
    });
    chstore::save(_path, doc);
    return m;
}

// ============================ DocumentRepository =============================
std::vector<Document> chdesktop::DocumentRepository::findByOwner(DocumentOwnerKind ownerKind, Id ownerId) const {
    json doc = chstore::load(_path);
    std::vector<Document> out;
    for (auto& o : doc["items"]) {
        Document d;
        d.id = integer(o, "id");
        d.ownerKind = documentOwnerKindFromString(str(o, "ownerKind", "component"));
        d.ownerId = integer(o, "ownerId");
        d.title = str(o, "title");
        d.category = str(o, "category");
        d.url = str(o, "url");
        d.notes = str(o, "notes");
        if (d.ownerKind == ownerKind && d.ownerId == ownerId) out.push_back(d);
    }
    return out;
}
Document chdesktop::DocumentRepository::save(Document d) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    json obj = {
        {"id", d.id}, {"ownerKind", toString(d.ownerKind)}, {"ownerId", d.ownerId},
        {"title", d.title}, {"category", d.category}, {"url", d.url}, {"notes", d.notes},
    };
    if (d.id == kNoId) { d.id = chstore::takeNextId(doc); obj["id"] = d.id; items.push_back(obj); }
    else if (json* found = findItem(items, d.id)) { *found = obj; }
    else { items.push_back(obj); }
    chstore::save(_path, doc);
    return d;
}
bool chdesktop::DocumentRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}

// ============================= ProjectRepository =============================
std::vector<Project> chdesktop::ProjectRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Project> out;
    for (auto& o : doc["items"]) {
        Project p;
        p.id = integer(o, "id");
        p.name = str(o, "name");
        p.description = str(o, "description");
        p.version = str(o, "version");
        p.firmware = str(o, "firmware");
        p.gitRepo = str(o, "gitRepo");
        p.status = str(o, "status");
        p.notes = str(o, "notes");
        out.push_back(p);
    }
    return out;
}
std::optional<Project> chdesktop::ProjectRepository::findById(Id id) const {
    for (auto& p : findAll()) if (p.id == id) return p;
    return std::nullopt;
}
Project chdesktop::ProjectRepository::save(Project p) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    json obj = {
        {"id", p.id}, {"name", p.name}, {"description", p.description}, {"version", p.version},
        {"firmware", p.firmware}, {"gitRepo", p.gitRepo}, {"status", p.status}, {"notes", p.notes},
    };
    if (p.id == kNoId) { p.id = chstore::takeNextId(doc); obj["id"] = p.id; items.push_back(obj); }
    else if (json* found = findItem(items, p.id)) { *found = obj; }
    else { items.push_back(obj); }
    chstore::save(_path, doc);
    return p;
}
bool chdesktop::ProjectRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}

// ======================== ProjectComponentRepository ========================
std::vector<ProjectComponent> chdesktop::ProjectComponentRepository::findByProject(Id projectId) const {
    json doc = chstore::load(_path);
    std::vector<ProjectComponent> out;
    for (auto& o : doc["items"]) {
        if (integer(o, "projectId") != projectId) continue;
        ProjectComponent link;
        link.id = integer(o, "id");
        link.projectId = integer(o, "projectId");
        link.componentId = integer(o, "componentId");
        link.label = str(o, "label");
        link.quantityRequired = integer(o, "quantityRequired", 1);
        out.push_back(link);
    }
    return out;
}
ProjectComponent chdesktop::ProjectComponentRepository::save(ProjectComponent link) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    json obj = {
        {"id", link.id}, {"projectId", link.projectId}, {"componentId", link.componentId},
        {"label", link.label}, {"quantityRequired", link.quantityRequired},
    };
    if (link.id == kNoId) { link.id = chstore::takeNextId(doc); obj["id"] = link.id; items.push_back(obj); }
    else if (json* found = findItem(items, link.id)) { *found = obj; }
    else { items.push_back(obj); }
    chstore::save(_path, doc);
    return link;
}
bool chdesktop::ProjectComponentRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}
bool chdesktop::ProjectComponentRepository::removeByProject(Id projectId) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    bool removed = false;
    for (auto it = items.begin(); it != items.end();) {
        if (integer(*it, "projectId") == projectId) { it = items.erase(it); removed = true; }
        else ++it;
    }
    if (removed) chstore::save(_path, doc);
    return removed;
}

// ============================ ReferentialRepository =========================
namespace {
RefItem refFrom(const json& o) {
    RefItem r;
    r.id = integer(o, "id");
    r.list = str(o, "list");
    r.value = str(o, "value");
    r.position = integer(o, "position");
    return r;
}
json refJson(const RefItem& r) {
    return {{"id", r.id}, {"list", r.list}, {"value", r.value}, {"position", r.position}};
}
} // namespace

std::vector<RefItem> chdesktop::ReferentialRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<RefItem> out;
    for (auto& o : doc["items"]) out.push_back(refFrom(o));
    return out;
}
std::vector<RefItem> chdesktop::ReferentialRepository::findByList(const std::string& list) const {
    std::vector<RefItem> out;
    for (auto& r : findAll()) if (r.list == list) out.push_back(r);
    std::sort(out.begin(), out.end(), [](const RefItem& a, const RefItem& b) {
        if (a.position != b.position) return a.position < b.position;
        return a.value < b.value;
    });
    return out;
}
RefItem chdesktop::ReferentialRepository::save(RefItem item) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    if (item.id == kNoId) { item.id = chstore::takeNextId(doc); items.push_back(refJson(item)); }
    else if (json* found = findItem(items, item.id)) { *found = refJson(item); }
    else { items.push_back(refJson(item)); }
    chstore::save(_path, doc);
    return item;
}
std::vector<RefItem> chdesktop::ReferentialRepository::saveAll(const std::vector<RefItem>& list) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    std::unordered_map<Id, size_t> index;
    for (size_t i = 0; i < items.size(); ++i) index[integer(items[i], "id")] = i;
    std::vector<RefItem> result;
    for (RefItem r : list) {
        if (r.id == kNoId) { r.id = chstore::takeNextId(doc); index[r.id] = items.size(); items.push_back(refJson(r)); }
        else {
            auto it = index.find(r.id);
            if (it != index.end()) items[it->second] = refJson(r);
            else { index[r.id] = items.size(); items.push_back(refJson(r)); }
        }
        result.push_back(r);
    }
    chstore::save(_path, doc);
    return result;
}
bool chdesktop::ReferentialRepository::remove(Id id) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    for (auto it = items.begin(); it != items.end(); ++it)
        if (integer(*it, "id") == id) { items.erase(it); chstore::save(_path, doc); return true; }
    return false;
}
