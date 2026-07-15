/**
 * Identifiants du domaine — aucune dépendance plateforme ici.
 */

#pragma once
#include <string>

namespace domain {

// Identité permanente d'une entité = UUID (v0.2). Portée par la valeur `id`
// elle-même : l'identifiant EST l'UUID, il survit à un changement de nom et ne
// collisionne pas entre postes (cf. HomeServerHub, docs/sync-contract.md).
// Généré via domain::generateUuid() (sync_meta.h).
using Id = std::string;
inline const Id kNoId{};   // chaîne vide = « pas d'identité »

} // namespace domain
