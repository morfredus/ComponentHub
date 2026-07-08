#pragma once
#include <vector>
#include <optional>
#include "project.h"
#include "project_component.h"

namespace domain {

class IProjectRepository {
public:
    virtual ~IProjectRepository() = default;
    virtual std::vector<Project> findAll() const = 0;
    virtual std::optional<Project> findById(Id id) const = 0;
    virtual Project save(Project project) = 0;
    virtual bool remove(Id id) = 0;
};

class IProjectComponentRepository {
public:
    virtual ~IProjectComponentRepository() = default;
    virtual std::vector<ProjectComponent> findByProject(Id projectId) const = 0;
    virtual ProjectComponent save(ProjectComponent link) = 0;
    virtual bool remove(Id id) = 0;
    virtual bool removeByProject(Id projectId) = 0;  // nettoyage en cascade à la suppression d'un projet
};

} // namespace domain
