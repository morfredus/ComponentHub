/**
 * Component — Fiche complète d'un composant électronique (cœur métier).
 *
 * Type de valeur pur : aucune dépendance plateforme. La couche storage/
 * (persistance) se charge de le convertir vers/depuis son format (JSON fichier).
 */

#pragma once
#include <string>
#include "ids.h"
#include "sync_meta.h"

namespace domain {

// Grande famille de l'objet — permet de traiter modules, outils et
// consommables comme des facettes filtrables du même inventaire plutôt que
// des systèmes séparés (cf. cahier des charges : "Filtres : ESP32, OLED,
// GPS, Raspberry, Outils, Capteurs, Modules récupérés, Stock faible").
enum class ComponentKind { Component, Module, Tool, Consumable };

inline const char* toString(ComponentKind k) {
    switch (k) {
        case ComponentKind::Component:  return "component";
        case ComponentKind::Module:     return "module";
        case ComponentKind::Tool:       return "tool";
        case ComponentKind::Consumable: return "consumable";
    }
    return "component";
}

inline ComponentKind componentKindFromString(const std::string& s) {
    if (s == "module")     return ComponentKind::Module;
    if (s == "tool")       return ComponentKind::Tool;
    if (s == "consumable") return ComponentKind::Consumable;
    return ComponentKind::Component;
}

struct Component {
    Id id = kNoId;

    // Enveloppe de synchronisation (identité permanente + métadonnées).
    // L'`id` entier ci-dessus reste la clé locale/UI en Phase 1 ; `meta.uuid`
    // porte l'identité inter-postes. Voir sync_meta.h.
    SyncMeta meta;

    ComponentKind kind = ComponentKind::Component;

    std::string name;
    std::string reference;
    std::string manufacturer;
    std::string description;
    std::string category;
    std::string subcategory;
    std::string type;

    std::string voltage;        // texte libre : "3.3V", "5-12V", ...
    std::string current;
    std::string interfaceType;  // "I2C", "SPI", "UART", "GPIO", ...
    std::string protocols;
    std::string i2cAddress;
    std::string frequency;
    int         pinCount = 0;

    std::string compatibility;

    double      price = 0.0;
    std::string supplier;
    std::string purchaseDate;   // ISO 8601 (YYYY-MM-DD)
    std::string receptionDate;
    std::string warranty;

    std::string state;              // état physique : neuf, occasion, HS...
    std::string status = "to_test"; // workflow de validation : to_test, validating, validated, defective, archived
    std::string origin;
    std::string notes;

    Id locationId = kNoId;

    int quantity   = 0;
    int minStock   = 0;
    int idealStock = 0;
};

} // namespace domain
