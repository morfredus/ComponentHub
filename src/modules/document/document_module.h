/**
 * DocumentModule — API REST générique pour les documents (datasheets,
 * manuels, schémas, liens...) rattachés à un composant ou un projet.
 */

#pragma once
#include "../../core/module.h"
#include "../../domain/document_service.h"
#include "../../storage/document_repository.h"

class DocumentModule : public Module {
public:
    const char* name() const override { return "Document"; }
    void registerRoutes(WebRouter& router) override;

private:
    DocumentJsonRepository _documentRepo;
    domain::DocumentService _service{_documentRepo};
};
