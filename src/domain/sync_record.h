/**
 * sync_record.h — Abstraction générique de synchronisation d'une table.
 *
 * Pour synchroniser TOUTES les entités (composants, emplacements, catégories,
 * projets, nomenclatures, référentiels, documents) sans dupliquer la logique,
 * chaque dépôt expose ses lignes sous une forme neutre : un `SyncRecord`
 * (identité + enveloppe + JSON brut de l'entité). Le SyncService ne connaît que
 * cette interface ; il ignore le métier de chaque table.
 */

#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

struct SyncRecord {
    Id id;
    std::string type;        // "component", "location", "category"…
    SyncMeta meta;           // enveloppe (rev, horodatage, tombstone)
    nlohmann::json data;     // objet JSON complet de l'entité (tel que stocké)
};

// Un dépôt synchronisable : expose ses lignes (tombstones inclus) et sait
// appliquer une ligne reçue du hub (highest-rev, verbatim).
class ISyncableRepository {
public:
    virtual ~ISyncableRepository() = default;
    virtual std::string syncType() const = 0;                 // discriminant de la table
    // Lignes modifiées localement depuis `sinceLocalSeq` (tombstones inclus).
    // sinceLocalSeq = 0 -> tout (première synchro).
    virtual std::vector<SyncRecord> exportForSync(std::int64_t sinceLocalSeq) const = 0;
    // Plus haute séquence locale attribuée (nouveau repère de push après envoi).
    virtual std::int64_t maxLocalSeq() const = 0;
    virtual bool applyRemote(const SyncRecord& record) = 0;    // true si le stockage a changé
};

} // namespace domain
