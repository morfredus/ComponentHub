/**
 * IReferentialRepository - access to the values of reference lists.
 *
 * A single interface for ALL lists (generic mechanism): the
 * `list` parameter selects the reference list concerned.
 */
#pragma once
#include <vector>
#include "referential.h"

namespace domain {

class IReferentialRepository {
public:
    virtual ~IReferentialRepository() = default;
    virtual std::vector<RefItem> findAll() const = 0;
    // Values of a reference list, sorted by `position` then alphabetically.
    virtual std::vector<RefItem> findByList(const std::string& list) const = 0;
    virtual RefItem save(RefItem item) = 0;
    virtual std::vector<RefItem> saveAll(const std::vector<RefItem>& items) = 0;  // batch upsert (reordering)
    virtual bool remove(Id id) = 0;
};

} // namespace domain
