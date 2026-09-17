// ComponentHub Desktop — implémentations SQLite des interfaces de dépôt du
// domaine (src/domain/repositories.h, project_repositories.h,
// document_repository.h). Chaque dépôt lit/écrit dans une base SQLite unique
// pour des performances supérieures et des requêtes complexes.
//
// Chaque table synchronisable implémente aussi domain::ISyncableRepository
// (sync_record.h) : le SyncService la traite de façon générique. StockMovement
// (historique append-only) n'est pas synchronisé.
//
// Le reste du cœur (services, CSV, import/export) vit dans src/domain/,
// indépendant de la plateforme.
#pragma once

#include <string>
#include <sqlite3.h>
#include "repositories.h"
#include "project_repositories.h"
#include "document_repository.h"
#include "referential_repository.h"
#include "sync_record.h"

namespace chdesktop {

// Macro d'aide : les trois méthodes de domain::ISyncableRepository, identiques
// à déclarer pour chaque dépôt.
#define CH_SYNCABLE_DECL                                                                  \
    std::string syncType() const override;                                               \
    std::vector<domain::SyncRecord> exportForSync(std::int64_t sinceLocalSeq) const override; \
    std::int64_t maxLocalSeq() const override;                                           \
    bool applyRemote(const domain::SyncRecord& record) override;

class SQLiteComponentRepository : public domain::IComponentRepository,
                                  public domain::ISyncableRepository {
public:
    explicit SQLiteComponentRepository(sqlite3* db);
    ~SQLiteComponentRepository() override = default;
    
    std::vector<domain::Component> findAll() const override;
    std::optional<domain::Component> findById(domain::Id id) const override;
    domain::Component save(domain::Component c) override;
    std::vector<domain::Component> saveAll(const std::vector<domain::Component>& cs) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

class SQLiteLocationRepository : public domain::ILocationRepository,
                                 public domain::ISyncableRepository {
public:
    explicit SQLiteLocationRepository(sqlite3* db);
    ~SQLiteLocationRepository() override = default;
    
    std::vector<domain::Location> findAll() const override;
    std::optional<domain::Location> findById(domain::Id id) const override;
    domain::Location save(domain::Location l) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

class SQLiteCategoryRepository : public domain::ICategoryRepository,
                                  public domain::ISyncableRepository {
public:
    explicit SQLiteCategoryRepository(sqlite3* db);
    ~SQLiteCategoryRepository() override = default;
    
    std::vector<domain::Category> findAll() const override;
    std::optional<domain::Category> findById(domain::Id id) const override;
    domain::Category save(domain::Category c) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

class SQLiteStockMovementRepository : public domain::IStockMovementRepository {
public:
    explicit SQLiteStockMovementRepository(sqlite3* db);
    ~SQLiteStockMovementRepository() override = default;
    
    std::vector<domain::StockMovement> findByComponent(domain::Id componentId) const override;
    domain::StockMovement add(domain::StockMovement m) override;
private:
    sqlite3* db_;
};

class SQLiteDocumentRepository : public domain::IDocumentRepository,
                                 public domain::ISyncableRepository {
public:
    explicit SQLiteDocumentRepository(sqlite3* db);
    ~SQLiteDocumentRepository() override = default;
    
    std::vector<domain::Document> findByOwner(domain::DocumentOwnerKind ownerKind, domain::Id ownerId) const override;
    domain::Document save(domain::Document d) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
    std::vector<std::string> referencedBlobs() const override;
private:
    sqlite3* db_;
};

class SQLiteProjectRepository : public domain::IProjectRepository,
                                public domain::ISyncableRepository {
public:
    explicit SQLiteProjectRepository(sqlite3* db);
    ~SQLiteProjectRepository() override = default;
    
    std::vector<domain::Project> findAll() const override;
    std::optional<domain::Project> findById(domain::Id id) const override;
    domain::Project save(domain::Project p) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

class SQLiteProjectComponentRepository : public domain::IProjectComponentRepository,
                                        public domain::ISyncableRepository {
public:
    explicit SQLiteProjectComponentRepository(sqlite3* db);
    ~SQLiteProjectComponentRepository() override = default;
    
    std::vector<domain::ProjectComponent> findByProject(domain::Id projectId) const override;
    domain::ProjectComponent save(domain::ProjectComponent link) override;
    bool remove(domain::Id id) override;
    bool removeByProject(domain::Id projectId) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

class SQLiteReferentialRepository : public domain::IReferentialRepository,
                                    public domain::ISyncableRepository {
public:
    explicit SQLiteReferentialRepository(sqlite3* db);
    ~SQLiteReferentialRepository() override = default;
    
    std::vector<domain::RefItem> findAll() const override;
    std::vector<domain::RefItem> findByList(const std::string& list) const override;
    domain::RefItem save(domain::RefItem item) override;
    std::vector<domain::RefItem> saveAll(const std::vector<domain::RefItem>& items) override;
    bool remove(domain::Id id) override;
    
    CH_SYNCABLE_DECL
private:
    sqlite3* db_;
};

// Gestionnaire de la base SQLite : ouverture et création du schéma initial.
class SQLiteDatabase {
public:
    explicit SQLiteDatabase(const std::string& dbPath);
    ~SQLiteDatabase();
    
    bool isOpen() const { return db_ != nullptr; }
    sqlite3* handle() { return db_; }
    
    // Initialise le schéma de la base vide.
    bool initialize();
    
private:
    bool createSchema();
    
    sqlite3* db_ = nullptr;
    std::string dbPath_;
};

} // namespace chdesktop
