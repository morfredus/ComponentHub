/**
 * Domain identifiers - no platform dependency here.
 */

#pragma once
#include <string>

namespace domain {

// Permanent identity of an entity = UUID (v0.2). Carried by the `id` value
// itself: the identifier IS the UUID, it survives a rename and does not
// collide between machines (see morfSync, docs/sync-contract.md).
// Generated via domain::generateUuid() (sync_meta.h).
using Id = std::string;
inline const Id kNoId{};   // empty string = "no identity"

} // namespace domain
