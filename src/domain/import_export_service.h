/**
 * ImportExportService - CSV export/import of the inventory.
 *
 * Two formats:
 *   - Native  : all Component columns, re-importable as is
 *     (backup/restore, spreadsheet editing). Lines are
 *     matched by `id` if present, otherwise always created.
 *   - Bomist  : compatible with the Bomist CSV export (columns Stock,
 *     Part Number, Manufacturer, Description, Label, Storage, Unit Cost,
 *     Pref. Supplier, Type, Category, Worth). Lines are matched
 *     by reference (Part Number): a match updates the
 *     existing component (Bomist fields only), otherwise a new
 *     component is created.
 *
 * `location`/`Storage` is a name resolved (or created) via ILocationRepository:
 * the CSV file never references an internal location id.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include "repositories.h"
#include "project_repositories.h"

namespace domain {

struct ImportResult {
    int created = 0;
    int updated = 0;
    int failed = 0;
    std::vector<std::string> errors;
};

enum class CsvFormat { Native, Bomist };

class ImportExportService {
public:
    ImportExportService(IComponentRepository& components, ILocationRepository& locations,
                        ICategoryRepository& categories, IProjectRepository& projects)
        : _components(components), _locations(locations),
          _categories(categories), _projects(projects) {}

    // Components (inventory) - native ComponentHub and Bomist formats.
    std::string exportCsv(CsvFormat format) const;
    ImportResult importCsv(const std::string& csv, CsvFormat format);

    // Secondary tables - one CSV per table, re-importable identically.
    // Locations export their hierarchy (parentId + readable path).
    std::string exportCategoriesCsv() const;
    std::string exportLocationsCsv() const;
    std::string exportProjectsCsv() const;
    ImportResult importCategoriesCsv(const std::string& csv);
    ImportResult importLocationsCsv(const std::string& csv);
    ImportResult importProjectsCsv(const std::string& csv);

private:
    IComponentRepository& _components;
    ILocationRepository&  _locations;
    ICategoryRepository&  _categories;
    IProjectRepository&   _projects;

    // `cache` is filled once per import (see _import*) then
    // reused for each line: avoids one ILocationRepository::findAll()
    // (hence a full disk re-read) per CSV line.
    Id _resolveLocationCached(const std::string& name, std::map<std::string, Id>& cache);
    std::string _locationName(Id id) const;
    // Readable hierarchical path "Workshop > Cabinet A > Drawer A12",
    // rebuilt from `parentId` using the provided index.
    std::string _locationPath(Id id, const std::map<Id, Location>& byId) const;

    std::string _exportNative() const;
    std::string _exportBomist() const;
    ImportResult _importNative(const std::string& csv);
    ImportResult _importBomist(const std::string& csv);
};

} // namespace domain
