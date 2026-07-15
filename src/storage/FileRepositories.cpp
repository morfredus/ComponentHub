#include "FileRepositories.h"
#include "JsonStore.h"

#include <algorithm>
#include <unordered_map>

using namespace domain;
using chstore::json;

namespace {

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
std::int64_t int64(const json& o, const char* k, std::int64_t def = 0) {
    auto it = o.find(k);
    return (it != o.end() && it->is_number()) ? it->get<std::int64_t>() : def;
}
bool boolean(const json& o, const char* k, bool def = false) {
    auto it = o.find(k);
    return (it != o.end() && it->is_boolean()) ? it->get<bool>() : def;
}
Id id(const json& o, const char* k = "id") { return str(o, k); }

// --- Enveloppe de synchronisation, commune à toutes les tables --------------
SyncMeta metaFrom(const json& o) {
    SyncMeta m;
    m.createdAt = str(o, "createdAt");
    m.updatedAt = str(o, "updatedAt");
    m.rev = int64(o, "rev");
    m.deleted = boolean(o, "deleted");
    m.localSeq = int64(o, "localSeq");
    return m;
}
void putMeta(json& o, const SyncMeta& m) {
    o["createdAt"] = m.createdAt;
    o["updatedAt"] = m.updatedAt;
    o["rev"] = m.rev;
    o["deleted"] = m.deleted;
    o["localSeq"] = m.localSeq;
}

// Compteur de séquence locale, persisté dans le document ("localSeq"). Attribué
// à chaque écriture locale (save/tombstone) pour repérer ce qui doit être poussé.
std::int64_t nextLocalSeq(json& doc) {
    std::int64_t next = doc.value("localSeq", static_cast<std::int64_t>(0)) + 1;
    doc["localSeq"] = next;
    return next;
}

// Trouve l'objet d'id donné dans le tableau "items" ; nullptr si absent.
json* findItem(json& items, const Id& wanted) {
    if (wanted == kNoId) return nullptr;
    for (auto& it : items) if (id(it) == wanted) return &it;
    return nullptr;
}

// --- Helpers génériques de synchronisation (indépendants du métier) ---------
// Export INCRÉMENTAL : seulement les lignes modifiées localement depuis
// `sinceLocalSeq` (tombstones inclus). Une ligne de synchro = l'objet JSON brut.
std::vector<SyncRecord> exportTable(const std::string& path, const std::string& type,
                                    std::int64_t sinceLocalSeq) {
    json doc = chstore::load(path);
    std::vector<SyncRecord> out;
    for (auto& o : doc["items"]) {
        if (int64(o, "localSeq") <= sinceLocalSeq) continue;  // inchangé depuis le dernier push
        SyncRecord r;
        r.id = id(o);
        r.type = type;
        r.meta = metaFrom(o);
        r.data = o;                 // objet complet, tel que stocké
        out.push_back(std::move(r));
    }
    return out;
}
// Plus haute séquence locale attribuée (= repère de push après un envoi réussi).
std::int64_t maxLocalSeqOf(const std::string& path) {
    json doc = chstore::load(path);
    return doc.value("localSeq", static_cast<std::int64_t>(0));
}
bool applyTable(const std::string& path, const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    json doc = chstore::load(path);
    json& items = doc["items"];
    if (json* found = findItem(items, rec.id)) {
        SyncMeta local = metaFrom(*found);
        const bool remoteNewer = (rec.meta.rev > local.rev) ||
            (rec.meta.rev == local.rev && rec.meta.updatedAt > local.updatedAt);
        if (!remoteNewer) return false;   // local déjà à jour / idempotent
        json item = rec.data;
        item["localSeq"] = 0;             // reçue du hub : neutralisée (pas de re-push)
        *found = item;
    } else {
        json item = rec.data;
        item["localSeq"] = 0;
        items.push_back(item);
    }
    chstore::save(path, doc);
    return true;
}
// Suppression logique : pose deleted=true + rafraîchit l'enveloppe + séquence locale.
bool tombstone(const std::string& path, const Id& wanted) {
    json doc = chstore::load(path);
    json& items = doc["items"];
    if (json* found = findItem(items, wanted)) {
        SyncMeta m = metaFrom(*found);
        if (m.deleted) return false;
        m.deleted = true;
        stampForSave(m);
        m.localSeq = nextLocalSeq(doc);   // à propager au prochain push
        putMeta(*found, m);
        chstore::save(path, doc);
        return true;
    }
    return false;
}

// --- Component <-> JSON ---
json toJson(const Component& c) {
    json o = {
        {"id", c.id},
        {"kind", toString(c.kind)}, {"name", c.name}, {"reference", c.reference},
        {"manufacturer", c.manufacturer}, {"description", c.description}, {"category", c.category},
        {"subcategory", c.subcategory}, {"type", c.type}, {"voltage", c.voltage}, {"current", c.current},
        {"interfaceType", c.interfaceType}, {"protocols", c.protocols}, {"i2cAddress", c.i2cAddress},
        {"frequency", c.frequency}, {"pinCount", c.pinCount}, {"compatibility", c.compatibility},
        {"price", c.price}, {"supplier", c.supplier}, {"purchaseDate", c.purchaseDate},
        {"receptionDate", c.receptionDate}, {"warranty", c.warranty}, {"state", c.state},
        {"status", c.status}, {"origin", c.origin}, {"notes", c.notes}, {"locationId", c.locationId},
        {"quantity", c.quantity}, {"minStock", c.minStock}, {"idealStock", c.idealStock},
    };
    putMeta(o, c.meta);
    return o;
}
Component componentFrom(const json& o) {
    Component c;
    c.id = id(o);
    c.meta = metaFrom(o);
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
    c.locationId = str(o, "locationId");
    c.quantity = integer(o, "quantity");
    c.minStock = integer(o, "minStock");
    c.idealStock = integer(o, "idealStock");
    return c;
}

} // namespace

// ============================ ComponentRepository ============================
std::vector<Component> chdesktop::ComponentRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Component> out;
    for (auto& o : doc["items"]) {
        Component c = componentFrom(o);
        if (!c.meta.deleted) out.push_back(c);   // tombstones masqués à l'appli
    }
    return out;
}
std::optional<Component> chdesktop::ComponentRepository::findById(Id wanted) const {
    for (auto& c : findAll()) if (c.id == wanted) return c;
    return std::nullopt;
}
Component chdesktop::ComponentRepository::save(Component c) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    stampForSave(c.meta);
    c.meta.localSeq = nextLocalSeq(doc);
    if (c.id == kNoId) { c.id = generateUuid(); items.push_back(toJson(c)); }
    else if (json* found = findItem(items, c.id)) { *found = toJson(c); }
    else { items.push_back(toJson(c)); }
    chstore::save(_path, doc);
    return c;
}
std::vector<Component> chdesktop::ComponentRepository::saveAll(const std::vector<Component>& cs) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    std::unordered_map<Id, size_t> index;
    for (size_t i = 0; i < items.size(); ++i) index[id(items[i])] = i;

    std::vector<Component> result;
    result.reserve(cs.size());
    for (Component c : cs) {
        stampForSave(c.meta);
        c.meta.localSeq = nextLocalSeq(doc);
        if (c.id == kNoId) {
            c.id = generateUuid();
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
bool chdesktop::ComponentRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::ComponentRepository::syncType() const { return "component"; }
std::vector<SyncRecord> chdesktop::ComponentRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "component", since); }
std::int64_t chdesktop::ComponentRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::ComponentRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ============================ LocationRepository =============================
std::vector<Location> chdesktop::LocationRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Location> out;
    for (auto& o : doc["items"]) {
        if (boolean(o, "deleted")) continue;
        Location l;
        l.id = id(o);
        l.meta = metaFrom(o);
        l.name = str(o, "name");
        l.parentId = str(o, "parentId");
        out.push_back(l);
    }
    return out;
}
std::optional<Location> chdesktop::LocationRepository::findById(Id wanted) const {
    for (auto& l : findAll()) if (l.id == wanted) return l;
    return std::nullopt;
}
Location chdesktop::LocationRepository::save(Location l) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    stampForSave(l.meta);
    l.meta.localSeq = nextLocalSeq(doc);
    if (l.id == kNoId) l.id = generateUuid();
    json obj = {{"id", l.id}, {"name", l.name}, {"parentId", l.parentId}};
    putMeta(obj, l.meta);
    if (json* found = findItem(items, l.id)) *found = obj;
    else items.push_back(obj);
    chstore::save(_path, doc);
    return l;
}
bool chdesktop::LocationRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::LocationRepository::syncType() const { return "location"; }
std::vector<SyncRecord> chdesktop::LocationRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "location", since); }
std::int64_t chdesktop::LocationRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::LocationRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ============================ CategoryRepository =============================
std::vector<Category> chdesktop::CategoryRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Category> out;
    for (auto& o : doc["items"]) {
        if (boolean(o, "deleted")) continue;
        Category c; c.id = id(o); c.meta = metaFrom(o); c.name = str(o, "name");
        out.push_back(c);
    }
    return out;
}
std::optional<Category> chdesktop::CategoryRepository::findById(Id wanted) const {
    for (auto& c : findAll()) if (c.id == wanted) return c;
    return std::nullopt;
}
Category chdesktop::CategoryRepository::save(Category c) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    stampForSave(c.meta);
    c.meta.localSeq = nextLocalSeq(doc);
    if (c.id == kNoId) c.id = generateUuid();
    json obj = {{"id", c.id}, {"name", c.name}};
    putMeta(obj, c.meta);
    if (json* found = findItem(items, c.id)) *found = obj;
    else items.push_back(obj);
    chstore::save(_path, doc);
    return c;
}
bool chdesktop::CategoryRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::CategoryRepository::syncType() const { return "category"; }
std::vector<SyncRecord> chdesktop::CategoryRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "category", since); }
std::int64_t chdesktop::CategoryRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::CategoryRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ========================= StockMovementRepository ==========================
// Historique append-only : NON synchronisé (pas d'enveloppe, pas de tombstone).
std::vector<StockMovement> chdesktop::StockMovementRepository::findByComponent(Id componentId) const {
    json doc = chstore::load(_path);
    std::vector<StockMovement> out;
    for (auto& o : doc["items"]) {
        if (str(o, "componentId") != componentId) continue;
        StockMovement m;
        m.id = id(o);
        m.componentId = str(o, "componentId");
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
    if (m.id == kNoId) m.id = generateUuid();
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
        if (boolean(o, "deleted")) continue;
        Document d;
        d.id = id(o);
        d.meta = metaFrom(o);
        d.ownerKind = documentOwnerKindFromString(str(o, "ownerKind", "component"));
        d.ownerId = str(o, "ownerId");
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
    stampForSave(d.meta);
    d.meta.localSeq = nextLocalSeq(doc);
    if (d.id == kNoId) d.id = generateUuid();
    json obj = {
        {"id", d.id}, {"ownerKind", toString(d.ownerKind)}, {"ownerId", d.ownerId},
        {"title", d.title}, {"category", d.category}, {"url", d.url}, {"notes", d.notes},
    };
    putMeta(obj, d.meta);
    if (json* found = findItem(items, d.id)) *found = obj;
    else items.push_back(obj);
    chstore::save(_path, doc);
    return d;
}
bool chdesktop::DocumentRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::DocumentRepository::syncType() const { return "document"; }
std::vector<SyncRecord> chdesktop::DocumentRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "document", since); }
std::int64_t chdesktop::DocumentRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::DocumentRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ============================= ProjectRepository =============================
std::vector<Project> chdesktop::ProjectRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<Project> out;
    for (auto& o : doc["items"]) {
        if (boolean(o, "deleted")) continue;
        Project p;
        p.id = id(o);
        p.meta = metaFrom(o);
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
std::optional<Project> chdesktop::ProjectRepository::findById(Id wanted) const {
    for (auto& p : findAll()) if (p.id == wanted) return p;
    return std::nullopt;
}
Project chdesktop::ProjectRepository::save(Project p) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    stampForSave(p.meta);
    p.meta.localSeq = nextLocalSeq(doc);
    if (p.id == kNoId) p.id = generateUuid();
    json obj = {
        {"id", p.id}, {"name", p.name}, {"description", p.description}, {"version", p.version},
        {"firmware", p.firmware}, {"gitRepo", p.gitRepo}, {"status", p.status}, {"notes", p.notes},
    };
    putMeta(obj, p.meta);
    if (json* found = findItem(items, p.id)) *found = obj;
    else items.push_back(obj);
    chstore::save(_path, doc);
    return p;
}
bool chdesktop::ProjectRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::ProjectRepository::syncType() const { return "project"; }
std::vector<SyncRecord> chdesktop::ProjectRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "project", since); }
std::int64_t chdesktop::ProjectRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::ProjectRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ======================== ProjectComponentRepository ========================
std::vector<ProjectComponent> chdesktop::ProjectComponentRepository::findByProject(Id projectId) const {
    json doc = chstore::load(_path);
    std::vector<ProjectComponent> out;
    for (auto& o : doc["items"]) {
        if (boolean(o, "deleted")) continue;
        if (str(o, "projectId") != projectId) continue;
        ProjectComponent link;
        link.id = id(o);
        link.meta = metaFrom(o);
        link.projectId = str(o, "projectId");
        link.componentId = str(o, "componentId");
        link.label = str(o, "label");
        link.quantityRequired = integer(o, "quantityRequired", 1);
        out.push_back(link);
    }
    return out;
}
ProjectComponent chdesktop::ProjectComponentRepository::save(ProjectComponent link) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    stampForSave(link.meta);
    link.meta.localSeq = nextLocalSeq(doc);
    if (link.id == kNoId) link.id = generateUuid();
    json obj = {
        {"id", link.id}, {"projectId", link.projectId}, {"componentId", link.componentId},
        {"label", link.label}, {"quantityRequired", link.quantityRequired},
    };
    putMeta(obj, link.meta);
    if (json* found = findItem(items, link.id)) *found = obj;
    else items.push_back(obj);
    chstore::save(_path, doc);
    return link;
}
bool chdesktop::ProjectComponentRepository::remove(Id wanted) { return tombstone(_path, wanted); }
bool chdesktop::ProjectComponentRepository::removeByProject(Id projectId) {
    // Tombstone en cascade de toutes les lignes du projet.
    json doc = chstore::load(_path);
    json& items = doc["items"];
    bool removed = false;
    for (auto& o : items) {
        if (str(o, "projectId") == projectId && !boolean(o, "deleted")) {
            SyncMeta m = metaFrom(o); m.deleted = true; stampForSave(m);
            m.localSeq = nextLocalSeq(doc); putMeta(o, m);
            removed = true;
        }
    }
    if (removed) chstore::save(_path, doc);
    return removed;
}

std::string chdesktop::ProjectComponentRepository::syncType() const { return "projectComponent"; }
std::vector<SyncRecord> chdesktop::ProjectComponentRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "projectComponent", since); }
std::int64_t chdesktop::ProjectComponentRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::ProjectComponentRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }

// ============================ ReferentialRepository =========================
namespace {
RefItem refFrom(const json& o) {
    RefItem r;
    r.id = id(o);
    r.meta = metaFrom(o);
    r.list = str(o, "list");
    r.value = str(o, "value");
    r.position = integer(o, "position");
    return r;
}
json refJson(const RefItem& r) {
    json o = {{"id", r.id}, {"list", r.list}, {"value", r.value}, {"position", r.position}};
    putMeta(o, r.meta);
    return o;
}
} // namespace

std::vector<RefItem> chdesktop::ReferentialRepository::findAll() const {
    json doc = chstore::load(_path);
    std::vector<RefItem> out;
    for (auto& o : doc["items"]) {
        if (boolean(o, "deleted")) continue;
        out.push_back(refFrom(o));
    }
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
    stampForSave(item.meta);
    item.meta.localSeq = nextLocalSeq(doc);
    if (item.id == kNoId) item.id = generateUuid();
    if (json* found = findItem(items, item.id)) *found = refJson(item);
    else items.push_back(refJson(item));
    chstore::save(_path, doc);
    return item;
}
std::vector<RefItem> chdesktop::ReferentialRepository::saveAll(const std::vector<RefItem>& list) {
    json doc = chstore::load(_path);
    json& items = doc["items"];
    std::unordered_map<Id, size_t> index;
    for (size_t i = 0; i < items.size(); ++i) index[id(items[i])] = i;
    std::vector<RefItem> result;
    for (RefItem r : list) {
        stampForSave(r.meta);
        r.meta.localSeq = nextLocalSeq(doc);
        if (r.id == kNoId) r.id = generateUuid();
        auto it = index.find(r.id);
        if (it != index.end()) items[it->second] = refJson(r);
        else { index[r.id] = items.size(); items.push_back(refJson(r)); }
        result.push_back(r);
    }
    chstore::save(_path, doc);
    return result;
}
bool chdesktop::ReferentialRepository::remove(Id wanted) { return tombstone(_path, wanted); }

std::string chdesktop::ReferentialRepository::syncType() const { return "refItem"; }
std::vector<SyncRecord> chdesktop::ReferentialRepository::exportForSync(std::int64_t since) const { return exportTable(_path, "refItem", since); }
std::int64_t chdesktop::ReferentialRepository::maxLocalSeq() const { return maxLocalSeqOf(_path); }
bool chdesktop::ReferentialRepository::applyRemote(const SyncRecord& r) { return applyTable(_path, r); }
