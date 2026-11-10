#include "SQLiteRepositories.h"

#include <sstream>
#include <nlohmann/json.hpp>
#include "sync_meta.h"
#include "json_tolerant.h"

using nlohmann::json;
using namespace domain;

namespace chdesktop {
namespace {

// Helper for SQLite errors
void checkSqlite(int rc, sqlite3* db, const std::string& operation) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
        std::string msg = "SQLite error in " + operation + ": " + sqlite3_errmsg(db);
        throw std::runtime_error(msg);
    }
}

// RAII guard of a statement: always finalizes, even when checkSqlite or a
// JSON conversion throws in the middle of a loop (otherwise the statement leaks
// and may lock the database).
class StmtGuard {
public:
    StmtGuard() = default;
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;
    ~StmtGuard() { finalize(); }
    sqlite3_stmt** operator&() { return &p_; }
    operator sqlite3_stmt*() const { return p_; }
    void finalize() { if (p_) { sqlite3_finalize(p_); p_ = nullptr; } }
private:
    sqlite3_stmt* p_ = nullptr;
};

// Tolerant text read: sqlite3_column_text returns nullptr for a NULL value,
// and building a std::string from nullptr is undefined behavior.
std::string colText(sqlite3_stmt* stmt, int i) {
    const unsigned char* t = sqlite3_column_text(stmt, i);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

// Helper to run a statement with no result
void exec(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

// --- Synchronization envelope (SyncMeta) ------------------------
SyncMeta metaFromSqlite(sqlite3_stmt* stmt, int offset) {
    SyncMeta m;
    m.createdAt = colText(stmt, offset + 0);
    m.updatedAt = colText(stmt, offset + 1);
    m.rev = sqlite3_column_int64(stmt, offset + 2);
    m.deleted = sqlite3_column_int(stmt, offset + 3) != 0;
    m.localSeq = sqlite3_column_int64(stmt, offset + 4);
    return m;
}

void bindMeta(sqlite3_stmt* stmt, int offset, const SyncMeta& m) {
    sqlite3_bind_text(stmt, offset + 0, m.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, offset + 1, m.updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, offset + 2, m.rev);
    sqlite3_bind_int(stmt, offset + 3, m.deleted ? 1 : 0);
    sqlite3_bind_int64(stmt, offset + 4, m.localSeq);
}

// A sequence is local to a table and must advance at every local
// change. The negative marker is reserved for PULL: it keeps the
// remote revision without preparing a bounce-back PUSH.
constexpr std::int64_t kRemoteSequence = -1;

void prepareMetaForSave(sqlite3* db, const char* table, SyncMeta& meta) {
    if (meta.localSeq == kRemoteSequence) {
        meta.localSeq = 0;
        return;
    }

    stampForSave(meta);
    const std::string sql = "SELECT COALESCE(MAX(localSeq), 0) + 1 FROM " +
                            std::string(table) + ";";
    StmtGuard stmt;
    checkSqlite(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), db,
                "prepare next local sequence");
    if (sqlite3_step(stmt) == SQLITE_ROW)
        meta.localSeq = sqlite3_column_int64(stmt, 0);
    stmt.finalize();
}

// --- Component <-> SQLite conversion ------------------------------------
json componentToJson(const Component& c) {
    return {
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
}

Component componentFromJson(const json& j) {
    Component c;
    c.id = jget(j, "id", std::string());
    c.kind = componentKindFromString(jget(j, "kind", "component"));
    c.name = jget(j, "name", "");
    c.reference = jget(j, "reference", "");
    c.manufacturer = jget(j, "manufacturer", "");
    c.description = jget(j, "description", "");
    c.category = jget(j, "category", "");
    c.subcategory = jget(j, "subcategory", "");
    c.type = jget(j, "type", "");
    c.voltage = jget(j, "voltage", "");
    c.current = jget(j, "current", "");
    c.interfaceType = jget(j, "interfaceType", "");
    c.protocols = jget(j, "protocols", "");
    c.i2cAddress = jget(j, "i2cAddress", "");
    c.frequency = jget(j, "frequency", "");
    c.pinCount = jget(j, "pinCount", 0);
    c.compatibility = jget(j, "compatibility", "");
    c.price = jget(j, "price", 0.0);
    c.supplier = jget(j, "supplier", "");
    c.purchaseDate = jget(j, "purchaseDate", "");
    c.receptionDate = jget(j, "receptionDate", "");
    c.warranty = jget(j, "warranty", "");
    c.state = jget(j, "state", "");
    c.status = jget(j, "status", "to_test");
    c.origin = jget(j, "origin", "");
    c.notes = jget(j, "notes", "");
    c.locationId = jget(j, "locationId", "");
    c.quantity = jget(j, "quantity", 0);
    c.minStock = jget(j, "minStock", 0);
    c.idealStock = jget(j, "idealStock", 0);
    return c;
}

} // namespace

// ==================== SQLiteDatabase ====================
SQLiteDatabase::SQLiteDatabase(const std::string& dbPath) : dbPath_(dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = "Cannot open database: " + std::string(sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(msg);
    }
}

SQLiteDatabase::~SQLiteDatabase() {
    if (db_) sqlite3_close(db_);
}

bool SQLiteDatabase::initialize() {
    // Enable foreign keys and WAL mode for better performance.
    // Errors go up to the startup point, which shows them to the user.
    configureConnection();
    exec(db_, "PRAGMA journal_mode = WAL;");   // persistent in the file: applies to all connections
    return createSchema();
}

void SQLiteDatabase::configureConnection() {
    // busy_timeout: several connections (UI + synchronization thread) write to the same database.
    // In WAL mode only one writer at a time; without a timeout, the second one gets SQLITE_BUSY at once
    // instead of waiting for the other's short transaction to end.
    sqlite3_busy_timeout(db_, kBusyTimeoutMs);
    exec(db_, "PRAGMA foreign_keys = ON;");
    exec(db_, "PRAGMA synchronous = NORMAL;");
}

bool SQLiteDatabase::openExisting() {
    // Auxiliary connection (worker thread): the schema and WAL mode already exist.
    configureConnection();
    return true;
}

void SQLiteDatabase::execSql(const std::string& sql) { exec(db_, sql); }

void SQLiteDatabase::snapshotTo(const std::string& destPath) {
    // The path is passed as a bound parameter: no SQL concatenation.
    StmtGuard stmt;
    checkSqlite(sqlite3_prepare_v2(db_, "VACUUM INTO ?;", -1, &stmt, nullptr), db_, "prepare snapshot");
    sqlite3_bind_text(stmt, 1, destPath.c_str(), -1, SQLITE_TRANSIENT);
    checkSqlite(sqlite3_step(stmt), db_, "snapshot");
}

bool SQLiteDatabase::createSchema() {
    // Table components
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS components (
            id TEXT PRIMARY KEY,
            kind TEXT NOT NULL,
            name TEXT NOT NULL,
            reference TEXT,
            manufacturer TEXT,
            description TEXT,
            category TEXT,
            subcategory TEXT,
            type TEXT,
            voltage TEXT,
            current TEXT,
            interfaceType TEXT,
            protocols TEXT,
            i2cAddress TEXT,
            frequency TEXT,
            pinCount INTEGER DEFAULT 0,
            compatibility TEXT,
            price REAL DEFAULT 0.0,
            supplier TEXT,
            purchaseDate TEXT,
            receptionDate TEXT,
            warranty TEXT,
            state TEXT,
            status TEXT DEFAULT 'to_test',
            origin TEXT,
            notes TEXT,
            locationId TEXT,
            quantity INTEGER DEFAULT 0,
            minStock INTEGER DEFAULT 0,
            idealStock INTEGER DEFAULT 0,
            -- Enveloppe de synchronisation
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table locations
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS locations (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            parentId TEXT,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table categories
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS categories (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table stock_movements
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS stock_movements (
            id TEXT PRIMARY KEY,
            componentId TEXT NOT NULL,
            type TEXT NOT NULL,
            quantity INTEGER NOT NULL,
            timestamp TEXT,
            note TEXT
        );
    )");

    // Table documents
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS documents (
            id TEXT PRIMARY KEY,
            ownerKind TEXT NOT NULL,
            ownerId TEXT NOT NULL,
            title TEXT NOT NULL,
            category TEXT,
            url TEXT,
            notes TEXT,
            blobHash TEXT,
            fileName TEXT,
            sizeBytes INTEGER DEFAULT 0,
            mime TEXT,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table projects
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS projects (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            version TEXT,
            firmware TEXT,
            gitRepo TEXT,
            status TEXT,
            notes TEXT,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table project_components
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS project_components (
            id TEXT PRIMARY KEY,
            projectId TEXT NOT NULL,
            componentId TEXT NOT NULL,
            label TEXT,
            quantityRequired INTEGER DEFAULT 1,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Table referentials
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS referentials (
            id TEXT PRIMARY KEY,
            list TEXT NOT NULL,
            value TEXT NOT NULL,
            position INTEGER DEFAULT 0,
            createdAt TEXT,
            updatedAt TEXT,
            rev INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            localSeq INTEGER DEFAULT 0
        );
    )");

    // Indexes for performance
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_components_location ON components(locationId);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_components_status ON components(status);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_locations_parent ON locations(parentId);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_documents_owner ON documents(ownerKind, ownerId);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_project_components_project ON project_components(projectId);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_referentials_list ON referentials(list);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_stock_movements_component ON stock_movements(componentId);");

    return true;
}

// ==================== SQLiteComponentRepository ====================
SQLiteComponentRepository::SQLiteComponentRepository(sqlite3* db) : db_(db) {}

std::vector<Component> SQLiteComponentRepository::findAll() const {
    std::vector<Component> result;
    
    const char* sql = "SELECT id, kind, name, reference, manufacturer, description, category, "
                     "subcategory, type, voltage, current, interfaceType, protocols, i2cAddress, "
                     "frequency, pinCount, compatibility, price, supplier, purchaseDate, "
                     "receptionDate, warranty, state, status, origin, notes, locationId, "
                     "quantity, minStock, idealStock, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM components WHERE deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findAll");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Component c;
        c.id = colText(stmt, 0);
        c.kind = componentKindFromString(colText(stmt, 1));
        c.name = colText(stmt, 2);
        c.reference = colText(stmt, 3);
        c.manufacturer = colText(stmt, 4);
        c.description = colText(stmt, 5);
        c.category = colText(stmt, 6);
        c.subcategory = colText(stmt, 7);
        c.type = colText(stmt, 8);
        c.voltage = colText(stmt, 9);
        c.current = colText(stmt, 10);
        c.interfaceType = colText(stmt, 11);
        c.protocols = colText(stmt, 12);
        c.i2cAddress = colText(stmt, 13);
        c.frequency = colText(stmt, 14);
        c.pinCount = sqlite3_column_int(stmt, 15);
        c.compatibility = colText(stmt, 16);
        c.price = sqlite3_column_double(stmt, 17);
        c.supplier = colText(stmt, 18);
        c.purchaseDate = colText(stmt, 19);
        c.receptionDate = colText(stmt, 20);
        c.warranty = colText(stmt, 21);
        c.state = colText(stmt, 22);
        c.status = colText(stmt, 23);
        c.origin = colText(stmt, 24);
        c.notes = colText(stmt, 25);
        c.locationId = colText(stmt, 26);
        c.quantity = sqlite3_column_int(stmt, 27);
        c.minStock = sqlite3_column_int(stmt, 28);
        c.idealStock = sqlite3_column_int(stmt, 29);
        c.meta = metaFromSqlite(stmt, 30);
        result.push_back(c);
    }
    
    stmt.finalize();
    return result;
}

std::optional<Component> SQLiteComponentRepository::findById(Id id) const {
    const char* sql = "SELECT id, kind, name, reference, manufacturer, description, category, "
                     "subcategory, type, voltage, current, interfaceType, protocols, i2cAddress, "
                     "frequency, pinCount, compatibility, price, supplier, purchaseDate, "
                     "receptionDate, warranty, state, status, origin, notes, locationId, "
                     "quantity, minStock, idealStock, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM components WHERE id = ? AND deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findById");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    std::optional<Component> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Component c;
        c.id = colText(stmt, 0);
        c.kind = componentKindFromString(colText(stmt, 1));
        c.name = colText(stmt, 2);
        c.reference = colText(stmt, 3);
        c.manufacturer = colText(stmt, 4);
        c.description = colText(stmt, 5);
        c.category = colText(stmt, 6);
        c.subcategory = colText(stmt, 7);
        c.type = colText(stmt, 8);
        c.voltage = colText(stmt, 9);
        c.current = colText(stmt, 10);
        c.interfaceType = colText(stmt, 11);
        c.protocols = colText(stmt, 12);
        c.i2cAddress = colText(stmt, 13);
        c.frequency = colText(stmt, 14);
        c.pinCount = sqlite3_column_int(stmt, 15);
        c.compatibility = colText(stmt, 16);
        c.price = sqlite3_column_double(stmt, 17);
        c.supplier = colText(stmt, 18);
        c.purchaseDate = colText(stmt, 19);
        c.receptionDate = colText(stmt, 20);
        c.warranty = colText(stmt, 21);
        c.state = colText(stmt, 22);
        c.status = colText(stmt, 23);
        c.origin = colText(stmt, 24);
        c.notes = colText(stmt, 25);
        c.locationId = colText(stmt, 26);
        c.quantity = sqlite3_column_int(stmt, 27);
        c.minStock = sqlite3_column_int(stmt, 28);
        c.idealStock = sqlite3_column_int(stmt, 29);
        c.meta = metaFromSqlite(stmt, 30);
        result = c;
    }
    
    stmt.finalize();
    return result;
}

Component SQLiteComponentRepository::save(Component c) {
    prepareMetaForSave(db_, "components", c.meta);
    
    const char* sql = "INSERT OR REPLACE INTO components (id, kind, name, reference, manufacturer, "
                     "description, category, subcategory, type, voltage, current, interfaceType, "
                     "protocols, i2cAddress, frequency, pinCount, compatibility, price, supplier, "
                     "purchaseDate, receptionDate, warranty, state, status, origin, notes, "
                     "locationId, quantity, minStock, idealStock, createdAt, updatedAt, rev, "
                     "deleted, localSeq) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                     "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (c.id == kNoId) c.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, c.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, toString(c.kind), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.reference.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.subcategory.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.voltage.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.current.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.interfaceType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.protocols.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.i2cAddress.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.frequency.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, c.pinCount);
    sqlite3_bind_text(stmt, idx++, c.compatibility.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, idx++, c.price);
    sqlite3_bind_text(stmt, idx++, c.supplier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.purchaseDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.receptionDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.warranty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.origin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.locationId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, c.quantity);
    sqlite3_bind_int(stmt, idx++, c.minStock);
    sqlite3_bind_int(stmt, idx++, c.idealStock);
    bindMeta(stmt, idx, c.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return c;
}

std::vector<Component> SQLiteComponentRepository::saveAll(const std::vector<Component>& cs) {
    std::vector<Component> result;
    result.reserve(cs.size());
    
    // Start a transaction for better performance
    exec(db_, "BEGIN TRANSACTION;");
    
    try {
        for (const auto& c : cs) {
            result.push_back(save(c));
        }
        exec(db_, "COMMIT;");
    } catch (...) {
        exec(db_, "ROLLBACK;");
        throw;
    }
    
    return result;
}

bool SQLiteComponentRepository::remove(Id id) {
    const char* sql = "UPDATE components SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM components) WHERE id = ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    
    return rc == SQLITE_DONE;
}

std::string SQLiteComponentRepository::syncType() const { return "component"; }
std::vector<SyncRecord> SQLiteComponentRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    
    const char* sql = "SELECT id, kind, name, reference, manufacturer, description, category, "
                     "subcategory, type, voltage, current, interfaceType, protocols, i2cAddress, "
                     "frequency, pinCount, compatibility, price, supplier, purchaseDate, "
                     "receptionDate, warranty, state, status, origin, notes, locationId, "
                     "quantity, minStock, idealStock, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM components WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "component";
        rec.meta = metaFromSqlite(stmt, 30);
        
        // Build the complete JSON object
        Component c;
        c.id = rec.id;
        c.kind = componentKindFromString(colText(stmt, 1));
        c.name = colText(stmt, 2);
        c.reference = colText(stmt, 3);
        c.manufacturer = colText(stmt, 4);
        c.description = colText(stmt, 5);
        c.category = colText(stmt, 6);
        c.subcategory = colText(stmt, 7);
        c.type = colText(stmt, 8);
        c.voltage = colText(stmt, 9);
        c.current = colText(stmt, 10);
        c.interfaceType = colText(stmt, 11);
        c.protocols = colText(stmt, 12);
        c.i2cAddress = colText(stmt, 13);
        c.frequency = colText(stmt, 14);
        c.pinCount = sqlite3_column_int(stmt, 15);
        c.compatibility = colText(stmt, 16);
        c.price = sqlite3_column_double(stmt, 17);
        c.supplier = colText(stmt, 18);
        c.purchaseDate = colText(stmt, 19);
        c.receptionDate = colText(stmt, 20);
        c.warranty = colText(stmt, 21);
        c.state = colText(stmt, 22);
        c.status = colText(stmt, 23);
        c.origin = colText(stmt, 24);
        c.notes = colText(stmt, 25);
        c.locationId = colText(stmt, 26);
        c.quantity = sqlite3_column_int(stmt, 27);
        c.minStock = sqlite3_column_int(stmt, 28);
        c.idealStock = sqlite3_column_int(stmt, 29);
        rec.data = componentToJson(c);
        
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteComponentRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM components;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    
    stmt.finalize();
    return result;
}

bool SQLiteComponentRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    // Check whether the entity exists and compare versions
    const char* checkSql = "SELECT rev, updatedAt FROM components WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    // Apply the remote data
    Component c = componentFromJson(rec.data);
    c.id = rec.id;
    c.meta = rec.meta;
    c.meta.localSeq = kRemoteSequence;
    
    return save(c).id == rec.id;
}

// ==================== SQLiteLocationRepository ====================
SQLiteLocationRepository::SQLiteLocationRepository(sqlite3* db) : db_(db) {}

std::vector<Location> SQLiteLocationRepository::findAll() const {
    std::vector<Location> result;
    const char* sql = "SELECT id, name, parentId, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM locations WHERE deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findAll");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Location l;
        l.id = colText(stmt, 0);
        l.name = colText(stmt, 1);
        l.parentId = colText(stmt, 2);
        l.meta = metaFromSqlite(stmt, 3);
        result.push_back(l);
    }
    
    stmt.finalize();
    return result;
}

std::optional<Location> SQLiteLocationRepository::findById(Id id) const {
    const char* sql = "SELECT id, name, parentId, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM locations WHERE id = ? AND deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findById");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    std::optional<Location> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Location l;
        l.id = colText(stmt, 0);
        l.name = colText(stmt, 1);
        l.parentId = colText(stmt, 2);
        l.meta = metaFromSqlite(stmt, 3);
        result = l;
    }
    
    stmt.finalize();
    return result;
}

Location SQLiteLocationRepository::save(Location l) {
    prepareMetaForSave(db_, "locations", l.meta);
    
    const char* sql = "INSERT OR REPLACE INTO locations (id, name, parentId, createdAt, updatedAt, rev, deleted, localSeq) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (l.id == kNoId) l.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, l.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, l.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, l.parentId.c_str(), -1, SQLITE_TRANSIENT);
    bindMeta(stmt, idx, l.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return l;
}

bool SQLiteLocationRepository::remove(Id id) {
    const char* sql = "UPDATE locations SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM locations) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteLocationRepository::syncType() const { return "location"; }
std::vector<SyncRecord> SQLiteLocationRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, name, parentId, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM locations WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "location";
        rec.meta = metaFromSqlite(stmt, 3);
        rec.data = {{"id", rec.id}, {"name", colText(stmt, 1)}, 
                   {"parentId", colText(stmt, 2)}};
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteLocationRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM locations;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteLocationRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM locations WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    Location l;
    l.id = rec.id;
    l.name = jget(rec.data, "name", "");
    l.parentId = jget(rec.data, "parentId", "");
    l.meta = rec.meta;
    l.meta.localSeq = kRemoteSequence;
    
    return save(l).id == rec.id;
}

// ==================== SQLiteCategoryRepository ====================
SQLiteCategoryRepository::SQLiteCategoryRepository(sqlite3* db) : db_(db) {}

std::vector<Category> SQLiteCategoryRepository::findAll() const {
    std::vector<Category> result;
    const char* sql = "SELECT id, name, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM categories WHERE deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findAll");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Category c;
        c.id = colText(stmt, 0);
        c.name = colText(stmt, 1);
        c.meta = metaFromSqlite(stmt, 2);
        result.push_back(c);
    }
    
    stmt.finalize();
    return result;
}

std::optional<Category> SQLiteCategoryRepository::findById(Id id) const {
    const char* sql = "SELECT id, name, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM categories WHERE id = ? AND deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findById");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    std::optional<Category> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Category c;
        c.id = colText(stmt, 0);
        c.name = colText(stmt, 1);
        c.meta = metaFromSqlite(stmt, 2);
        result = c;
    }
    
    stmt.finalize();
    return result;
}

Category SQLiteCategoryRepository::save(Category c) {
    prepareMetaForSave(db_, "categories", c.meta);
    
    const char* sql = "INSERT OR REPLACE INTO categories (id, name, createdAt, updatedAt, rev, deleted, localSeq) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (c.id == kNoId) c.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, c.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, c.name.c_str(), -1, SQLITE_TRANSIENT);
    bindMeta(stmt, idx, c.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return c;
}

bool SQLiteCategoryRepository::remove(Id id) {
    const char* sql = "UPDATE categories SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM categories) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteCategoryRepository::syncType() const { return "category"; }
std::vector<SyncRecord> SQLiteCategoryRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, name, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM categories WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "category";
        rec.meta = metaFromSqlite(stmt, 2);
        rec.data = {{"id", rec.id}, {"name", colText(stmt, 1)}};
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteCategoryRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM categories;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteCategoryRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM categories WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    Category c;
    c.id = rec.id;
    c.name = jget(rec.data, "name", "");
    c.meta = rec.meta;
    c.meta.localSeq = kRemoteSequence;
    
    return save(c).id == rec.id;
}

// ==================== SQLiteStockMovementRepository ====================
SQLiteStockMovementRepository::SQLiteStockMovementRepository(sqlite3* db) : db_(db) {}

std::vector<StockMovement> SQLiteStockMovementRepository::findByComponent(Id componentId) const {
    std::vector<StockMovement> result;
    const char* sql = "SELECT id, componentId, type, quantity, timestamp, note "
                     "FROM stock_movements WHERE componentId = ? ORDER BY timestamp DESC;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findByComponent");
    sqlite3_bind_text(stmt, 1, componentId.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StockMovement m;
        m.id = colText(stmt, 0);
        m.componentId = colText(stmt, 1);
        m.type = stockMovementTypeFromString(colText(stmt, 2));
        m.quantity = sqlite3_column_int(stmt, 3);
        m.timestamp = colText(stmt, 4);
        m.note = colText(stmt, 5);
        result.push_back(m);
    }
    
    stmt.finalize();
    return result;
}

StockMovement SQLiteStockMovementRepository::add(StockMovement m) {
    if (m.id == kNoId) m.id = generateUuid();
    
    const char* sql = "INSERT INTO stock_movements (id, componentId, type, quantity, timestamp, note) "
                     "VALUES (?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare add");
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, m.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, m.componentId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, toString(m.type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, m.quantity);
    sqlite3_bind_text(stmt, idx++, m.timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, m.note.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step add");
    stmt.finalize();
    
    return m;
}

// ==================== SQLiteDocumentRepository ====================
SQLiteDocumentRepository::SQLiteDocumentRepository(sqlite3* db) : db_(db) {}

std::vector<Document> SQLiteDocumentRepository::findByOwner(DocumentOwnerKind ownerKind, Id ownerId) const {
    std::vector<Document> result;
    const char* sql = "SELECT id, ownerKind, ownerId, title, category, url, notes, blobHash, fileName, sizeBytes, mime, "
                     "createdAt, updatedAt, rev, deleted, localSeq FROM documents WHERE deleted = 0 AND ownerKind = ? AND ownerId = ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findByOwner");
    sqlite3_bind_text(stmt, 1, toString(ownerKind), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ownerId.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Document d;
        d.id = colText(stmt, 0);
        d.ownerKind = documentOwnerKindFromString(colText(stmt, 1));
        d.ownerId = colText(stmt, 2);
        d.title = colText(stmt, 3);
        d.category = colText(stmt, 4);
        d.url = colText(stmt, 5);
        d.notes = colText(stmt, 6);
        d.blobHash = colText(stmt, 7);
        d.fileName = colText(stmt, 8);
        d.sizeBytes = sqlite3_column_int64(stmt, 9);
        d.mime = colText(stmt, 10);
        d.meta = metaFromSqlite(stmt, 11);
        result.push_back(d);
    }
    
    stmt.finalize();
    return result;
}

Document SQLiteDocumentRepository::save(Document d) {
    prepareMetaForSave(db_, "documents", d.meta);
    
    const char* sql = "INSERT OR REPLACE INTO documents (id, ownerKind, ownerId, title, category, url, notes, blobHash, fileName, sizeBytes, mime, "
                     "createdAt, updatedAt, rev, deleted, localSeq) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (d.id == kNoId) d.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, d.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, toString(d.ownerKind), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.ownerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.blobHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, d.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, d.sizeBytes);
    sqlite3_bind_text(stmt, idx++, d.mime.c_str(), -1, SQLITE_TRANSIENT);
    bindMeta(stmt, idx, d.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return d;
}

bool SQLiteDocumentRepository::remove(Id id) {
    const char* sql = "UPDATE documents SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM documents) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteDocumentRepository::syncType() const { return "document"; }
std::vector<SyncRecord> SQLiteDocumentRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, ownerKind, ownerId, title, category, url, notes, blobHash, fileName, sizeBytes, mime, "
                     "createdAt, updatedAt, rev, deleted, localSeq FROM documents WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "document";
        rec.meta = metaFromSqlite(stmt, 11);
        rec.data = {
            {"id", rec.id},
            {"ownerKind", colText(stmt, 1)},
            {"ownerId", colText(stmt, 2)},
            {"title", colText(stmt, 3)},
            {"category", colText(stmt, 4)},
            {"url", colText(stmt, 5)},
            {"notes", colText(stmt, 6)},
            {"blobHash", colText(stmt, 7)},
            {"fileName", colText(stmt, 8)},
            {"sizeBytes", sqlite3_column_int64(stmt, 9)},
            {"mime", colText(stmt, 10)}
        };
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteDocumentRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM documents;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteDocumentRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM documents WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    Document d;
    d.id = rec.id;
    d.ownerKind = documentOwnerKindFromString(jget(rec.data, "ownerKind", "component"));
    d.ownerId = jget(rec.data, "ownerId", "");
    d.title = jget(rec.data, "title", "");
    d.category = jget(rec.data, "category", "");
    d.url = jget(rec.data, "url", "");
    d.notes = jget(rec.data, "notes", "");
    d.blobHash = jget(rec.data, "blobHash", "");
    d.fileName = jget(rec.data, "fileName", "");
    d.sizeBytes = jget(rec.data, "sizeBytes", std::int64_t(0));
    d.mime = jget(rec.data, "mime", "");
    d.meta = rec.meta;
    d.meta.localSeq = kRemoteSequence;
    
    return save(d).id == rec.id;
}

std::vector<std::string> SQLiteDocumentRepository::referencedBlobs() const {
    std::vector<std::string> result;
    const char* sql = "SELECT blobHash FROM documents WHERE deleted = 0 AND blobHash IS NOT NULL AND blobHash != '';";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare referencedBlobs");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string hash = colText(stmt, 0);
        if (!hash.empty()) result.push_back(std::move(hash));
    }
    
    stmt.finalize();
    return result;
}

// ==================== SQLiteProjectRepository ====================
SQLiteProjectRepository::SQLiteProjectRepository(sqlite3* db) : db_(db) {}

std::vector<Project> SQLiteProjectRepository::findAll() const {
    std::vector<Project> result;
    const char* sql = "SELECT id, name, description, version, firmware, gitRepo, status, notes, "
                     "createdAt, updatedAt, rev, deleted, localSeq FROM projects WHERE deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findAll");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Project p;
        p.id = colText(stmt, 0);
        p.name = colText(stmt, 1);
        p.description = colText(stmt, 2);
        p.version = colText(stmt, 3);
        p.firmware = colText(stmt, 4);
        p.gitRepo = colText(stmt, 5);
        p.status = colText(stmt, 6);
        p.notes = colText(stmt, 7);
        p.meta = metaFromSqlite(stmt, 8);
        result.push_back(p);
    }
    
    stmt.finalize();
    return result;
}

std::optional<Project> SQLiteProjectRepository::findById(Id id) const {
    const char* sql = "SELECT id, name, description, version, firmware, gitRepo, status, notes, "
                     "createdAt, updatedAt, rev, deleted, localSeq FROM projects WHERE id = ? AND deleted = 0;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findById");
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    std::optional<Project> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Project p;
        p.id = colText(stmt, 0);
        p.name = colText(stmt, 1);
        p.description = colText(stmt, 2);
        p.version = colText(stmt, 3);
        p.firmware = colText(stmt, 4);
        p.gitRepo = colText(stmt, 5);
        p.status = colText(stmt, 6);
        p.notes = colText(stmt, 7);
        p.meta = metaFromSqlite(stmt, 8);
        result = p;
    }
    
    stmt.finalize();
    return result;
}

Project SQLiteProjectRepository::save(Project p) {
    prepareMetaForSave(db_, "projects", p.meta);
    
    const char* sql = "INSERT OR REPLACE INTO projects (id, name, description, version, firmware, gitRepo, status, notes, "
                     "createdAt, updatedAt, rev, deleted, localSeq) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (p.id == kNoId) p.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, p.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.firmware.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.gitRepo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, p.notes.c_str(), -1, SQLITE_TRANSIENT);
    bindMeta(stmt, idx, p.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return p;
}

bool SQLiteProjectRepository::remove(Id id) {
    const char* sql = "UPDATE projects SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM projects) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteProjectRepository::syncType() const { return "project"; }
std::vector<SyncRecord> SQLiteProjectRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, name, description, version, firmware, gitRepo, status, notes, "
                     "createdAt, updatedAt, rev, deleted, localSeq FROM projects WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "project";
        rec.meta = metaFromSqlite(stmt, 8);
        rec.data = {
            {"id", rec.id},
            {"name", colText(stmt, 1)},
            {"description", colText(stmt, 2)},
            {"version", colText(stmt, 3)},
            {"firmware", colText(stmt, 4)},
            {"gitRepo", colText(stmt, 5)},
            {"status", colText(stmt, 6)},
            {"notes", colText(stmt, 7)}
        };
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteProjectRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM projects;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteProjectRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM projects WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    Project p;
    p.id = rec.id;
    p.name = jget(rec.data, "name", "");
    p.description = jget(rec.data, "description", "");
    p.version = jget(rec.data, "version", "");
    p.firmware = jget(rec.data, "firmware", "");
    p.gitRepo = jget(rec.data, "gitRepo", "");
    p.status = jget(rec.data, "status", "");
    p.notes = jget(rec.data, "notes", "");
    p.meta = rec.meta;
    p.meta.localSeq = kRemoteSequence;
    
    return save(p).id == rec.id;
}

// ==================== SQLiteProjectComponentRepository ====================
SQLiteProjectComponentRepository::SQLiteProjectComponentRepository(sqlite3* db) : db_(db) {}

std::vector<ProjectComponent> SQLiteProjectComponentRepository::findByProject(Id projectId) const {
    std::vector<ProjectComponent> result;
    const char* sql = "SELECT id, projectId, componentId, label, quantityRequired, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM project_components WHERE deleted = 0 AND projectId = ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findByProject");
    sqlite3_bind_text(stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ProjectComponent link;
        link.id = colText(stmt, 0);
        link.projectId = colText(stmt, 1);
        link.componentId = colText(stmt, 2);
        link.label = colText(stmt, 3);
        link.quantityRequired = sqlite3_column_int(stmt, 4);
        link.meta = metaFromSqlite(stmt, 5);
        result.push_back(link);
    }
    
    stmt.finalize();
    return result;
}

ProjectComponent SQLiteProjectComponentRepository::save(ProjectComponent link) {
    prepareMetaForSave(db_, "project_components", link.meta);
    
    const char* sql = "INSERT OR REPLACE INTO project_components (id, projectId, componentId, label, quantityRequired, "
                     "createdAt, updatedAt, rev, deleted, localSeq) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (link.id == kNoId) link.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, link.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, link.projectId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, link.componentId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, link.label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, link.quantityRequired);
    bindMeta(stmt, idx, link.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return link;
}

bool SQLiteProjectComponentRepository::remove(Id id) {
    const char* sql = "UPDATE project_components SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM project_components) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

bool SQLiteProjectComponentRepository::removeByProject(Id projectId) {
    const char* sql = "UPDATE project_components SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM project_components) WHERE projectId = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare removeByProject");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, projectId.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteProjectComponentRepository::syncType() const { return "project_component"; }
std::vector<SyncRecord> SQLiteProjectComponentRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, projectId, componentId, label, quantityRequired, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM project_components WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "project_component";
        rec.meta = metaFromSqlite(stmt, 5);
        rec.data = {
            {"id", rec.id},
            {"projectId", colText(stmt, 1)},
            {"componentId", colText(stmt, 2)},
            {"label", colText(stmt, 3)},
            {"quantityRequired", sqlite3_column_int(stmt, 4)}
        };
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteProjectComponentRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM project_components;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteProjectComponentRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM project_components WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    ProjectComponent link;
    link.id = rec.id;
    link.projectId = jget(rec.data, "projectId", "");
    link.componentId = jget(rec.data, "componentId", "");
    link.label = jget(rec.data, "label", "");
    link.quantityRequired = jget(rec.data, "quantityRequired", 1);
    link.meta = rec.meta;
    link.meta.localSeq = kRemoteSequence;
    
    return save(link).id == rec.id;
}

// ==================== SQLiteReferentialRepository ====================
SQLiteReferentialRepository::SQLiteReferentialRepository(sqlite3* db) : db_(db) {}

std::vector<RefItem> SQLiteReferentialRepository::findAll() const {
    std::vector<RefItem> result;
    const char* sql = "SELECT id, list, value, position, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM referentials WHERE deleted = 0 ORDER BY list, position;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findAll");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RefItem item;
        item.id = colText(stmt, 0);
        item.list = colText(stmt, 1);
        item.value = colText(stmt, 2);
        item.position = sqlite3_column_int(stmt, 3);
        item.meta = metaFromSqlite(stmt, 4);
        result.push_back(item);
    }
    
    stmt.finalize();
    return result;
}

std::vector<RefItem> SQLiteReferentialRepository::findByList(const std::string& list) const {
    std::vector<RefItem> result;
    const char* sql = "SELECT id, list, value, position, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM referentials WHERE deleted = 0 AND list = ? ORDER BY position;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare findByList");
    sqlite3_bind_text(stmt, 1, list.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RefItem item;
        item.id = colText(stmt, 0);
        item.list = colText(stmt, 1);
        item.value = colText(stmt, 2);
        item.position = sqlite3_column_int(stmt, 3);
        item.meta = metaFromSqlite(stmt, 4);
        result.push_back(item);
    }
    
    stmt.finalize();
    return result;
}

RefItem SQLiteReferentialRepository::save(RefItem item) {
    prepareMetaForSave(db_, "referentials", item.meta);
    
    const char* sql = "INSERT OR REPLACE INTO referentials (id, list, value, position, createdAt, updatedAt, rev, deleted, localSeq) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare save");
    
    if (item.id == kNoId) item.id = generateUuid();
    
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, item.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, item.list.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, item.value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, item.position);
    bindMeta(stmt, idx, item.meta);
    
    rc = sqlite3_step(stmt);
    checkSqlite(rc, db_, "step save");
    stmt.finalize();
    
    return item;
}

std::vector<RefItem> SQLiteReferentialRepository::saveAll(const std::vector<RefItem>& items) {
    std::vector<RefItem> result;
    result.reserve(items.size());
    
    exec(db_, "BEGIN TRANSACTION;");
    
    try {
        for (const auto& item : items) {
            result.push_back(save(item));
        }
        exec(db_, "COMMIT;");
    } catch (...) {
        exec(db_, "ROLLBACK;");
        throw;
    }
    
    return result;
}

bool SQLiteReferentialRepository::remove(Id id) {
    const char* sql = "UPDATE referentials SET deleted = 1, updatedAt = ?, rev = rev + 1, localSeq = "
                      "(SELECT COALESCE(MAX(localSeq), 0) + 1 FROM referentials) WHERE id = ?;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare remove");
    const std::string updatedAt = nowIso8601Utc();
    sqlite3_bind_text(stmt, 1, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    stmt.finalize();
    return rc == SQLITE_DONE;
}

std::string SQLiteReferentialRepository::syncType() const { return "referential"; }
std::vector<SyncRecord> SQLiteReferentialRepository::exportForSync(std::int64_t since) const {
    std::vector<SyncRecord> result;
    const char* sql = "SELECT id, list, value, position, createdAt, updatedAt, rev, deleted, localSeq "
                     "FROM referentials WHERE localSeq > ?;";
    
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare exportForSync");
    sqlite3_bind_int64(stmt, 1, since);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncRecord rec;
        rec.id = colText(stmt, 0);
        rec.type = "referential";
        rec.meta = metaFromSqlite(stmt, 4);
        rec.data = {
            {"id", rec.id},
            {"list", colText(stmt, 1)},
            {"value", colText(stmt, 2)},
            {"position", sqlite3_column_int(stmt, 3)}
        };
        result.push_back(rec);
    }
    
    stmt.finalize();
    return result;
}

std::int64_t SQLiteReferentialRepository::maxLocalSeq() const {
    const char* sql = "SELECT MAX(localSeq) FROM referentials;";
    StmtGuard stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    checkSqlite(rc, db_, "prepare maxLocalSeq");
    
    std::int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    stmt.finalize();
    return result;
}

bool SQLiteReferentialRepository::applyRemote(const SyncRecord& rec) {
    if (rec.id == kNoId) return false;
    
    const char* checkSql = "SELECT rev, updatedAt FROM referentials WHERE id = ?;";
    StmtGuard checkStmt;
    int rc = sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr);
    checkSqlite(rc, db_, "prepare checkRemote");
    sqlite3_bind_text(checkStmt, 1, rec.id.c_str(), -1, SQLITE_TRANSIENT);
    
    bool shouldApply = true;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        std::int64_t localRev = sqlite3_column_int64(checkStmt, 0);
        std::string localUpdatedAt = colText(checkStmt, 1);
        const bool remoteNewer = (rec.meta.rev > localRev) ||
            (rec.meta.rev == localRev && rec.meta.updatedAt > localUpdatedAt);
        shouldApply = remoteNewer;
    }
    checkStmt.finalize();
    
    if (!shouldApply) return false;
    
    RefItem item;
    item.id = rec.id;
    item.list = jget(rec.data, "list", "");
    item.value = jget(rec.data, "value", "");
    item.position = jget(rec.data, "position", 0);
    item.meta = rec.meta;
    item.meta.localSeq = kRemoteSequence;
    
    return save(item).id == rec.id;
}

} // namespace chdesktop
