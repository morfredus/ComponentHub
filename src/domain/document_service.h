/**
 * DocumentService — Façade métier au-dessus d'IDocumentRepository.
 *
 * Volontairement fine (aucune règle métier au-delà du CRUD) : conservée pour
 * rester cohérent avec le reste du domaine (le module n'appelle jamais un
 * dépôt directement) et pour absorber une future règle sans casser l'API.
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
