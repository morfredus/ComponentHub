#pragma once
#include "../domain/project_repositories.h"

class ProjectJsonRepository : public domain::IProjectRepository {
public:
    explicit ProjectJsonRepository(const char* path = "/projects.json") : _path(path) {}

    std::vector<domain::Project> findAll() const override;
    std::optional<domain::Project> findById(domain::Id id) const override;
    domain::Project save(domain::Project project) override;
    bool remove(domain::Id id) override;

private:
    const char* _path;
};
