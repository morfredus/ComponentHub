#pragma once
#include "../domain/repositories.h"

class LocationJsonRepository : public domain::ILocationRepository {
public:
    explicit LocationJsonRepository(const char* path = "/inventory_locations.json") : _path(path) {}

    std::vector<domain::Location> findAll() const override;
    std::optional<domain::Location> findById(domain::Id id) const override;
    domain::Location save(domain::Location location) override;
    bool remove(domain::Id id) override;

private:
    const char* _path;
};
