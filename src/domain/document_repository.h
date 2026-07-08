#pragma once
#include <vector>
#include "document.h"

namespace domain {

class IDocumentRepository {
public:
    virtual ~IDocumentRepository() = default;
    virtual std::vector<Document> findByOwner(DocumentOwnerKind ownerKind, Id ownerId) const = 0;
    virtual Document save(Document document) = 0;
    virtual bool remove(Id id) = 0;
};

} // namespace domain
