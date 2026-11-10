/**
 * Document - "Documentation" attachment (datasheet, manual, schematic,
 * pinout, useful link...) attached to a component or a project.
 *
 * `url` points either to an external link (http...) or to a local file
 * (path on disk): no dedicated upload pipeline is duplicated here.
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

    // --- "File" attachment (optional) ------------------------------
    // A document is either a LINK (http... url set, empty blobHash) or an
    // imported FILE, whose content lives in the attachment store
    // under this hash. A non-empty `blobHash` TELLS THEM APART: an external link is
    // already synchronized as is, a file needs its binary to travel too
    // (morfSync blob store). Backward compatible: documents created before
    // this version have no blobHash and remain links.
    std::string  blobHash;      // SHA-256 of the content; empty => the document is a link
    std::string  fileName;      // original name (display + extension when opening)
    std::int64_t sizeBytes = 0; // size of the content
    std::string  mime;          // type, optional (deduced from the extension when needed)

    bool isFile() const { return !blobHash.empty(); }
};

} // namespace domain
