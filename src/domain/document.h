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

    // --- Pièce jointe « fichier » (facultatif) ------------------------------
    // Un document est soit un LIEN (url http... renseignée, blobHash vide), soit
    // un FICHIER importé, dont le contenu vit dans le magasin de pièces jointes
    // sous ce hash. `blobHash` non vide DISTINGUE les deux : un lien externe se
    // synchronise déjà tel quel, un fichier a besoin que son binaire voyage
    // (magasin de blobs morfSync). Rétrocompatible : les documents créés avant
    // cette version n'ont pas de blobHash et restent des liens.
    std::string  blobHash;      // SHA-256 du contenu ; vide => document = lien
    std::string  fileName;      // nom d'origine (affichage + extension à l'ouverture)
    std::int64_t sizeBytes = 0; // taille du contenu
    std::string  mime;          // type, facultatif (déduit de l'extension au besoin)

    bool isFile() const { return !blobHash.empty(); }
};

} // namespace domain
