/**
 * DocumentService - Business facade on top of IDocumentRepository.
 *
 * Deliberately thin (no business rule beyond CRUD): kept to
 * stay consistent with the rest of the domain (the module never calls a
 * repository directly) and to absorb a future rule without breaking the API.
 */

#pragma once
#include "document_repository.h"

namespace domain {

class DocumentService {
public:
    explicit DocumentService(IDocumentRepository& documents) : _documents(documents) {}

    std::vector<Document> listForOwner(DocumentOwnerKind ownerKind, Id ownerId) const {
        return _documents.findByOwner(ownerKind, ownerId);
    }

    Document save(Document document) { return _documents.save(std::move(document)); }
    bool remove(Id id) { return _documents.remove(id); }

private:
    IDocumentRepository& _documents;
};

} // namespace domain
