/**
 * json_tolerant.h - JSON reading that never throws on a badly typed field.
 *
 * Why: json::value("notes", "") throws type_error if the hub sends "notes": null
 * (or a number instead of a text). In PULL, a single exception of this kind
 * blocked synchronization forever. Here, an absent, null or wrongly typed field
 * -> default value.
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

// Overload for literals: jget(j, "k", "") returns a std::string.
inline std::string jget(const nlohmann::json& j, const char* key, const char* def) {
    return jget<std::string>(j, key, std::string(def));
}

} // namespace domain
