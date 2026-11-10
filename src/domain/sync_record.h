/**
 * sync_record.h - Generic abstraction for synchronizing a table.
 *
 * To synchronize ALL entities (components, locations, categories,
 * projects, bills of materials, reference lists, documents) without duplicating the logic,
 * each repository exposes its rows in a neutral form: a `SyncRecord`
 * (identity + envelope + raw JSON of the entity). SyncService only knows
 * this interface; it ignores the business meaning of each table.
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
    nlohmann::json data;     // complete JSON object of the entity (as stored)
};

// A synchronizable repository: exposes its rows (tombstones included) and knows how to
// apply a row received from the hub (highest-rev, verbatim).
class ISyncableRepository {
public:
    virtual ~ISyncableRepository() = default;
    virtual std::string syncType() const = 0;                 // table discriminator
    // Rows changed locally since `sinceLocalSeq` (tombstones included).
    // sinceLocalSeq = 0 -> everything (first synchronization).
    virtual std::vector<SyncRecord> exportForSync(std::int64_t sinceLocalSeq) const = 0;
    // Highest local sequence assigned (new push marker after sending).
    virtual std::int64_t maxLocalSeq() const = 0;
    virtual bool applyRemote(const SyncRecord& record) = 0;    // true if the storage changed

    // Hashes of the blobs (attachments) that this table references and whose
    // BINARY must travel separately (blob store, see sync_record on the
    // document side). By default no table references any: only the documents
    // repository overrides this method. Generic: SyncService transfers the
    // blobs without knowing which table carries them.
    virtual std::vector<std::string> referencedBlobs() const { return {}; }
};

} // namespace domain
