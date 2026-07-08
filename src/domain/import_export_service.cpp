#include "import_export_service.h"
#include "csv.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace domain {

namespace {

std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Tolère "11,00 €", "1234.56", "" ... : ne garde que chiffres/séparateurs,
// traite le dernier séparateur rencontré comme le point décimal.
double parsePriceFr(const std::string& raw) {
    std::string cleaned;
    for (char c : raw) {
        if (isdigit((unsigned char)c) || c == '.' || c == ',' || c == '-') cleaned.push_back(c);
    }
    if (cleaned.empty()) return 0.0;

    size_t lastSep = cleaned.find_last_of(".,");
    std::string normalized;
    for (size_t i = 0; i < cleaned.size(); i++) {
        char c = cleaned[i];
        if (c == '.' || c == ',') { if (i == lastSep) normalized.push_back('.'); }
        else normalized.push_back(c);
    }
    return strtod(normalized.c_str(), nullptr);
}

std::string formatPriceFr(double value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", value);
    std::string s(buf);
    for (char& c : s) if (c == '.') c = ',';
    return s + " €";
}

int parseIntSafe(const std::string& s, int def = 0) {
    if (s.empty()) return def;
    return (int)strtol(s.c_str(), nullptr, 10);
}

std::string intStr(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return std::string(buf);
}

bool isBlankRow(const std::vector<std::string>& row) {
    for (auto& f : row) if (!f.empty()) return false;
    return true;
}

std::string cell(const std::vector<std::string>& row, const std::map<std::string, int>& idx, const std::string& col) {
    auto it = idx.find(col);
    if (it == idx.end() || (size_t)it->second >= row.size()) return "";
    return row[it->second];
}

std::map<std::string, int> headerIndex(const std::vector<std::string>& header) {
    std::map<std::string, int> idx;
    for (size_t i = 0; i < header.size(); i++) idx[header[i]] = (int)i;
    return idx;
}

} // namespace

Id ImportExportService::_resolveLocationCached(const std::string& name, std::map<std::string, Id>& cache) {
    if (name.empty()) return kNoId;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;

    Location loc;
    loc.name = name;
    Id id = _locations.save(loc).id;
    cache[name] = id;
    return id;
}

std::string ImportExportService::_locationName(Id id) const {
    if (id == kNoId) return "";
    auto loc = _locations.findById(id);
    return loc ? loc->name : "";
}

std::string ImportExportService::exportCsv(CsvFormat format) const {
    return format == CsvFormat::Bomist ? _exportBomist() : _exportNative();
}

ImportResult ImportExportService::importCsv(const std::string& csv, CsvFormat format) {
    return format == CsvFormat::Bomist ? _importBomist(csv) : _importNative(csv);
}

std::string ImportExportService::_exportNative() const {
    std::string out = joinCsvRow({
        "id", "kind", "name", "reference", "manufacturer", "description", "category", "subcategory", "type",
        "voltage", "current", "interfaceType", "protocols", "i2cAddress", "frequency", "pinCount", "compatibility",
        "price", "supplier", "purchaseDate", "receptionDate", "warranty", "state", "status", "origin", "notes",
        "location", "quantity", "minStock", "idealStock"
    }) + "\n";

    for (auto& c : _components.findAll()) {
        out += joinCsvRow({
            intStr(c.id), toString(c.kind), c.name, c.reference, c.manufacturer, c.description, c.category,
            c.subcategory, c.type, c.voltage, c.current, c.interfaceType, c.protocols, c.i2cAddress,
            c.frequency, intStr(c.pinCount), c.compatibility, formatPriceFr(c.price), c.supplier,
            c.purchaseDate, c.receptionDate, c.warranty, c.state, c.status, c.origin, c.notes,
            _locationName(c.locationId), intStr(c.quantity), intStr(c.minStock), intStr(c.idealStock)
        }) + "\n";
    }
    return out;
}

std::string ImportExportService::_exportBomist() const {
    std::string out = joinCsvRow({
        "Stock", "Part Number", "Manufacturer", "Description", "Label", "Storage",
        "Unit Cost", "Pref. Supplier", "Type", "Category", "Worth"
    }) + "\n";

    for (auto& c : _components.findAll()) {
        out += joinCsvRow({
            intStr(c.quantity), c.reference, c.manufacturer, c.description, c.subcategory,
            _locationName(c.locationId), formatPriceFr(c.price), c.supplier, c.type, c.category,
            formatPriceFr(c.price * c.quantity)
        }) + "\n";
    }
    return out;
}

ImportResult ImportExportService::_importNative(const std::string& csv) {
    ImportResult result;
    auto rows = parseCsv(csv, ';');
    if (rows.empty()) return result;

    auto idx = headerIndex(rows[0]);

    // Tout le rapprochement se fait en mémoire à partir d'un seul instantané
    // disque (findAll), et l'écriture se fait en un seul lot (saveAll) à la
    // fin : un appel save()/findAll() par ligne de CSV relit et réécrit tout
    // le fichier à chaque itération, un coût qui explose avec le nombre de
    // lignes (déjà observé en usage réel : reset watchdog après ~150s pour
    // un import de 39 lignes).
    std::map<Id, bool> existingIds;
    for (auto& existing : _components.findAll()) existingIds[existing.id] = true;

    std::map<std::string, Id> locationCache;
    for (auto& l : _locations.findAll()) locationCache[l.name] = l.id;

    std::vector<Component> toSave;
    toSave.reserve(rows.size());

    for (size_t r = 1; r < rows.size(); r++) {
        auto& row = rows[r];
        if (isBlankRow(row)) continue;

        std::string name = cell(row, idx, "name");
        if (name.empty()) {
            result.failed++;
            result.errors.push_back("ligne " + intStr((int)r + 1) + ": nom manquant");
            continue;
        }

        Component c;
        c.id = parseIntSafe(cell(row, idx, "id"));
        c.kind = componentKindFromString(cell(row, idx, "kind"));
        c.name = name;
        c.reference = cell(row, idx, "reference");
        c.manufacturer = cell(row, idx, "manufacturer");
        c.description = cell(row, idx, "description");
        c.category = cell(row, idx, "category");
        c.subcategory = cell(row, idx, "subcategory");
        c.type = cell(row, idx, "type");
        c.voltage = cell(row, idx, "voltage");
        c.current = cell(row, idx, "current");
        c.interfaceType = cell(row, idx, "interfaceType");
        c.protocols = cell(row, idx, "protocols");
        c.i2cAddress = cell(row, idx, "i2cAddress");
        c.frequency = cell(row, idx, "frequency");
        c.pinCount = parseIntSafe(cell(row, idx, "pinCount"));
        c.compatibility = cell(row, idx, "compatibility");
        c.price = parsePriceFr(cell(row, idx, "price"));
        c.supplier = cell(row, idx, "supplier");
        c.purchaseDate = cell(row, idx, "purchaseDate");
        c.receptionDate = cell(row, idx, "receptionDate");
        c.warranty = cell(row, idx, "warranty");
        c.state = cell(row, idx, "state");
        std::string status = cell(row, idx, "status");
        c.status = status.empty() ? "to_test" : status;
        c.origin = cell(row, idx, "origin");
        c.notes = cell(row, idx, "notes");
        c.locationId = _resolveLocationCached(cell(row, idx, "location"), locationCache);
        c.quantity = parseIntSafe(cell(row, idx, "quantity"));
        c.minStock = parseIntSafe(cell(row, idx, "minStock"));
        c.idealStock = parseIntSafe(cell(row, idx, "idealStock"));

        if (c.id != kNoId && existingIds.count(c.id)) result.updated++; else result.created++;
        toSave.push_back(c);
    }

    _components.saveAll(toSave);
    return result;
}

ImportResult ImportExportService::_importBomist(const std::string& csv) {
    ImportResult result;
    auto rows = parseCsv(csv, ';');
    if (rows.empty()) return result;

    auto idx = headerIndex(rows[0]);

    // Même principe que _importNative : un seul instantané disque, tout le
    // rapprochement (par référence) en mémoire, une seule écriture en lot.
    std::map<std::string, Component> byReference;
    for (auto& existing : _components.findAll()) byReference[toLowerStr(existing.reference)] = existing;

    std::map<std::string, Id> locationCache;
    for (auto& l : _locations.findAll()) locationCache[l.name] = l.id;

    std::vector<Component> toSave;
    toSave.reserve(rows.size());
    // Index des lignes déjà ajoutées à toSave pour CE lot, par référence :
    // si le fichier contient deux fois le même Part Number (export mal
    // formé), la seconde occurrence met à jour la même entrée en attente
    // plutôt que de créer un second composant.
    std::map<std::string, size_t> pendingIndexByReference;

    for (size_t r = 1; r < rows.size(); r++) {
        auto& row = rows[r];
        if (isBlankRow(row)) continue;

        std::string reference = cell(row, idx, "Part Number");
        if (reference.empty()) {
            result.failed++;
            result.errors.push_back("ligne " + intStr((int)r + 1) + ": Part Number manquant");
            continue;
        }
        std::string lowerRef = toLowerStr(reference);

        Component c;
        auto pendingIt = pendingIndexByReference.find(lowerRef);
        bool isPending = pendingIt != pendingIndexByReference.end();
        if (isPending) {
            c = toSave[pendingIt->second];
        } else {
            auto it = byReference.find(lowerRef);
            if (it != byReference.end()) c = it->second;
            else {
                c.reference = reference;
                c.kind = ComponentKind::Component;
                c.status = "to_test";
            }
        }
        if (c.name.empty()) c.name = reference;

        c.quantity = parseIntSafe(cell(row, idx, "Stock"));
        c.manufacturer = cell(row, idx, "Manufacturer");
        c.description = cell(row, idx, "Description");
        c.subcategory = cell(row, idx, "Label");
        c.locationId = _resolveLocationCached(cell(row, idx, "Storage"), locationCache);
        c.price = parsePriceFr(cell(row, idx, "Unit Cost"));
        c.supplier = cell(row, idx, "Pref. Supplier");
        c.type = cell(row, idx, "Type");
        c.category = cell(row, idx, "Category");

        if (isPending) {
            toSave[pendingIt->second] = c;
        } else {
            if (c.id != kNoId) result.updated++; else result.created++;
            pendingIndexByReference[lowerRef] = toSave.size();
            toSave.push_back(c);
        }
    }

    _components.saveAll(toSave);
    return result;
}

} // namespace domain
