// ComponentHub Desktop — implémentations fichier des interfaces de dépôt du
// domaine (src/domain/repositories.h, project_repositories.h,
// document_repository.h). Chaque dépôt lit/écrit un fichier JSON (voir
// JsonStore.h).
//
// Le reste du cœur (services, CSV, import/export) vit dans src/domain/,
// indépendant de la plateforme.
#pragma once

#include <string>
#include "repositories.h"
#include "project_repositories.h"
#include "document_repository.h"
#include "referential_repository.h"

namespace chdesktop {

class ComponentRepository : public domain::IComponentRepository {
public:
    explicit ComponentRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::Component> findAll() const override;
    std::optional<domain::Component> findById(domain::Id id) const override;
    domain::Component save(domain::Component c) override;
    std::vector<domain::Component> saveAll(const std::vector<domain::Component>& cs) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

class LocationRepository : public domain::ILocationRepository {
public:
    explicit LocationRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::Location> findAll() const override;
    std::optional<domain::Location> findById(domain::Id id) const override;
    domain::Location save(domain::Location l) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

class CategoryRepository : public domain::ICategoryRepository {
public:
    explicit CategoryRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::Category> findAll() const override;
    std::optional<domain::Category> findById(domain::Id id) const override;
    domain::Category save(domain::Category c) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

class StockMovementRepository : public domain::IStockMovementRepository {
public:
    explicit StockMovementRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::StockMovement> findByComponent(domain::Id componentId) const override;
    domain::StockMovement add(domain::StockMovement m) override;
private:
    std::string _path;
};

class DocumentRepository : public domain::IDocumentRepository {
public:
    explicit DocumentRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::Document> findByOwner(domain::DocumentOwnerKind ownerKind, domain::Id ownerId) const override;
    domain::Document save(domain::Document d) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

class ProjectRepository : public domain::IProjectRepository {
public:
    explicit ProjectRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::Project> findAll() const override;
    std::optional<domain::Project> findById(domain::Id id) const override;
    domain::Project save(domain::Project p) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

class ProjectComponentRepository : public domain::IProjectComponentRepository {
public:
    explicit ProjectComponentRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::ProjectComponent> findByProject(domain::Id projectId) const override;
    domain::ProjectComponent save(domain::ProjectComponent link) override;
    bool remove(domain::Id id) override;
    bool removeByProject(domain::Id projectId) override;
private:
    std::string _path;
};

class ReferentialRepository : public domain::IReferentialRepository {
public:
    explicit ReferentialRepository(std::string path) : _path(std::move(path)) {}
    std::vector<domain::RefItem> findAll() const override;
    std::vector<domain::RefItem> findByList(const std::string& list) const override;
    domain::RefItem save(domain::RefItem item) override;
    std::vector<domain::RefItem> saveAll(const std::vector<domain::RefItem>& items) override;
    bool remove(domain::Id id) override;
private:
    std::string _path;
};

} // namespace chdesktop
