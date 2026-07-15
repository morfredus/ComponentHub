/**
 * Document — Pièce jointe "documentation" (datasheet, manuel, schéma,
 * pinout, lien utile...) rattachée à un composant ou à un projet.
 *
 * `url` pointe soit vers un lien externe (http...), soit vers un fichier local
 * (chemin sur le disque) : aucun pipeline d'upload dédié n'est dupliqué ici.
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

enum class DocumentOwnerKind { Component, Project };

inline const char* toString(DocumentOwnerKind k) {
    switch (k) {
        case DocumentOwnerKind::Component: return "component";
        case DocumentOwnerKind::Project:   return "project";
    }
    return "component";
}

inline DocumentOwnerKind documentOwnerKindFromString(const std::string& s) {
    if (s == "project") return DocumentOwnerKind::Project;
    return DocumentOwnerKind::Component;
}

struct Document {
    Id id = kNoId;
    SyncMeta meta;        // enveloppe de synchronisation
    DocumentOwnerKind ownerKind = DocumentOwnerKind::Component;
    Id ownerId = kNoId;

    std::string title;
    std::string category;  // datasheet, manual, schematic, pinout, link, other
    std::string url;
    std::string notes;
};

} // namespace domain
