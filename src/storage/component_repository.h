#pragma once
#include "../domain/repositories.h"

class ComponentJsonRepository : public domain::IComponentRepository {
public:
    explicit ComponentJsonRepository(const char* path = "/inventory_components.json") : _path(path) {}

    std::vector<domain::Component> findAll() const override;
    std::optional<domain::Component> findById(domain::Id id) const override;
    domain::Component save(domain::Component component) override;
    std::vector<domain::Component> saveAll(const std::vector<domain::Component>& components) override;
    bool remove(domain::Id id) override;

private:
    const char* _path;
};
