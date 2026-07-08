/**
 * ImportExportService — Export/import CSV de l'inventaire.
 *
 * Deux formats :
 *   - Native  : toutes les colonnes de Component, réimportables tel quel
 *     (sauvegarde/restauration, édition tableur). Les lignes sont
 *     rapprochées par `id` si présent, sinon toujours créées.
 *   - Bomist  : compatible avec l'export CSV de Bomist (colonnes Stock,
 *     Part Number, Manufacturer, Description, Label, Storage, Unit Cost,
 *     Pref. Supplier, Type, Category, Worth). Les lignes sont rapprochées
 *     par référence (Part Number) : une correspondance met à jour le
 *     composant existant (champs Bomist uniquement), sinon un nouveau
 *     composant est créé.
 *
 * `location`/`Storage` est un nom résolu (ou créé) via ILocationRepository :
 * le fichier CSV ne référence jamais un id d'emplacement interne.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include "repositories.h"

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
    ImportExportService(IComponentRepository& components, ILocationRepository& locations)
        : _components(components), _locations(locations) {}

    std::string exportCsv(CsvFormat format) const;
    ImportResult importCsv(const std::string& csv, CsvFormat format);

private:
    IComponentRepository& _components;
    ILocationRepository&  _locations;

    // `cache` est alimenté une seule fois par import (voir _import*) puis
    // réutilisé pour chaque ligne : évite un ILocationRepository::findAll()
    // (donc une relecture disque complète) par ligne de CSV.
    Id _resolveLocationCached(const std::string& name, std::map<std::string, Id>& cache);
    std::string _locationName(Id id) const;

    std::string _exportNative() const;
    std::string _exportBomist() const;
    ImportResult _importNative(const std::string& csv);
    ImportResult _importBomist(const std::string& csv);
};

} // namespace domain
