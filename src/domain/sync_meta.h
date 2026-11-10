/**
 * sync_meta.h - Synchronization envelope of an entity (see morfSync,
 * docs/sync-contract.md §1). Pure value type, no platform dependency.
 *
 * PERMANENT identity = the entity's `id`, which IS a UUID since v0.2
 * (Phase 2: see ids.h). SyncMeta therefore only carries the
 * synchronization metadata (revision, timestamp, tombstone), not the identity.
 */

#pragma once
#include <string>
#include <cstdint>
#include <random>
#include <ctime>
#include <cstdio>

namespace domain {

struct SyncMeta {
    std::string  createdAt;    // ISO 8601 UTC
    std::string  updatedAt;    // ISO 8601 UTC
    std::int64_t rev = 0;      // incremented at every save()
    bool         deleted = false; // tombstone (suppression logique)
    // LOCAL sequence (specific to this machine, not synchronized): incremented at
    // every local change. Lets the client PUSH only the entities
    // changed since its last upload (0 = received from the hub, nothing to push back).
    std::int64_t localSeq = 0;
};

// Generates a v4 (random) UUID. Enough for an entity identity: the
// probability of a collision is negligible at the scale of a personal inventory.
inline std::string generateUuid() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    std::uint64_t a = dist(rng), b = dist(rng);
    // Sets the version (4) and the variant (10xx) according to RFC 4122.
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<unsigned>(a >> 32),
        static_cast<unsigned>((a >> 16) & 0xFFFF),
        static_cast<unsigned>(a & 0xFFFF),
        static_cast<unsigned>(b >> 48),
        static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    return buf;
}

// Current timestamp in ISO 8601 UTC (e.g. "2026-07-15T09:30:00Z").
inline std::string nowIso8601Utc() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Prepares the meta for a write: sets createdAt if needed, refreshes
// updatedAt and increments rev. (The identity/uuid is carried by the `id`, assigned
// separately when saving.)
inline void stampForSave(SyncMeta& m) {
    const std::string now = nowIso8601Utc();
    if (m.createdAt.empty()) m.createdAt = now;
    m.updatedAt = now;
    ++m.rev;
}

} // namespace domain
