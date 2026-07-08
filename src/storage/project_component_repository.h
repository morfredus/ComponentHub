#pragma once
#include "../domain/project_repositories.h"

class ProjectComponentJsonRepository : public domain::IProjectComponentRepository {
public:
    explicit ProjectComponentJsonRepository(const char* path = "/project_components.json") : _path(path) {}

    std::vector<domain::ProjectComponent> findByProject(domain::Id projectId) const override;
    domain::ProjectComponent save(domain::ProjectComponent link) override;
    bool remove(domain::Id id) override;
    bool removeByProject(domain::Id projectId) override;

private:
    const char* _path;
};
