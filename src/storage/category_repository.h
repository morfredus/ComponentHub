#pragma once
#include "../domain/repositories.h"

class CategoryJsonRepository : public domain::ICategoryRepository {
public:
    explicit CategoryJsonRepository(const char* path = "/inventory_categories.json") : _path(path) {}

    std::vector<domain::Category> findAll() const override;
    std::optional<domain::Category> findById(domain::Id id) const override;
    domain::Category save(domain::Category category) override;
    bool remove(domain::Id id) override;

private:
    const char* _path;
};
