#pragma once
#include "../domain/document_repository.h"

class DocumentJsonRepository : public domain::IDocumentRepository {
public:
    explicit DocumentJsonRepository(const char* path = "/inventory_documents.json") : _path(path) {}

    std::vector<domain::Document> findByOwner(domain::DocumentOwnerKind ownerKind, domain::Id ownerId) const override;
    domain::Document save(domain::Document document) override;
    bool remove(domain::Id id) override;

private:
    const char* _path;
};
