/**
 * json_tolerant.h - lecture JSON qui ne lève jamais sur un champ mal typé.
 *
 * Pourquoi : json::value("notes", "") lève type_error si le hub envoie "notes": null
 * (ou un nombre à la place d'un texte). Dans le PULL, une seule exception de ce genre
 * bloquait la synchronisation pour toujours. Ici, champ absent, null ou de mauvais type
 * -> valeur par défaut.
 */
#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace domain {

template <class T>
T jget(const nlohmann::json& j, const char* key, T def) {
    if (!j.is_object()) return def;
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    try { return it->get<T>(); }
    catch (const nlohmann::json::exception&) { return def; }
}

// Surcharge pour les littéraux : jget(j, "k", "") renvoie un std::string.
inline std::string jget(const nlohmann::json& j, const char* key, const char* def) {
    return jget<std::string>(j, key, std::string(def));
}

} // namespace domain
