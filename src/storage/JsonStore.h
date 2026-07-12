// ComponentHub Desktop — persistance JSON.
//
// Réplique EXACTEMENT le format de fichier de la version ESP32
// (src/storage/json_store_util.h) : chaque table est un fichier
//   { "nextId": <int>, "items": [ {...}, ... ] }
// Conséquence voulue : les fichiers de données et les sauvegardes TAR sont
// interchangeables entre l'ESP32 et cette application de bureau.
//
// Seule cette couche connaît nlohmann/json ; le domaine (src/domain/) ne voit
// que des std::vector<Component> etc.
#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace chstore {

using nlohmann::json;

// Charge un fichier table. Un fichier absent ou illisible (corrompu) renvoie
// une structure vide valide plutôt que de propager une exception.
// Les chemins sont des std::string en UTF-8 (issus de QString::toStdString()).
// On les ouvre via std::filesystem::u8path pour que Windows retrouve bien un
// dossier de données au nom accentué (sinon std::ifstream interpréterait la
// chaîne en page de code ANSI et échouerait).
inline std::filesystem::path fsPath(const std::string& p) {
    return std::filesystem::u8path(p);
}

inline json load(const std::string& path) {
    json doc;
    std::ifstream in(fsPath(path), std::ios::binary);
    if (in) {
        try { in >> doc; } catch (...) { doc = json::object(); }
    }
    if (!doc.is_object()) doc = json::object();
    if (!doc.contains("items") || !doc["items"].is_array()) doc["items"] = json::array();
    if (!doc.contains("nextId") || !doc["nextId"].is_number_integer()) doc["nextId"] = 1;
    return doc;
}

// Écriture atomique : fichier temporaire puis rename par-dessus l'original —
// une coupure en cours d'écriture laisse le fichier existant intact (même
// principe que la version ESP32).
inline bool save(const std::string& path, const json& doc) {
    const std::filesystem::path target = fsPath(path);
    const std::filesystem::path tmp = fsPath(path + ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << doc.dump(2);
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        // Sur certains systèmes, rename échoue si la cible existe : on retente
        // après suppression explicite de l'ancienne version.
        std::filesystem::remove(target, ec);
        std::filesystem::rename(tmp, target, ec);
    }
    if (ec) { std::error_code rm; std::filesystem::remove(tmp, rm); return false; }
    return true;
}

inline int takeNextId(json& doc) {
    int id = doc.value("nextId", 1);
    doc["nextId"] = id + 1;
    return id;
}

} // namespace chstore
