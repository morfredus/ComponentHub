/**
 * sync_meta.h — Enveloppe de synchronisation d'une entité (cf. HomeServerHub,
 * docs/sync-contract.md §1). Type de valeur pur, aucune dépendance plateforme.
 *
 * Identité PERMANENTE = l'`id` de l'entité, qui EST un UUID depuis la v0.2
 * (Phase 2 : voir ids.h). SyncMeta ne porte donc plus que les métadonnées de
 * synchronisation (révision, horodatage, tombstone), pas l'identité.
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
    std::int64_t rev = 0;      // incrémenté à chaque save()
    bool         deleted = false; // tombstone (suppression logique)
    // Séquence LOCALE (propre à ce poste, non synchronisée) : incrémentée à
    // chaque modification locale. Permet au client de ne PUSH que les entités
    // changées depuis son dernier envoi (0 = reçue du hub, rien à repousser).
    std::int64_t localSeq = 0;
};

// Génère un UUID v4 (aléatoire). Suffisant pour une identité d'entité : la
// probabilité de collision est négligeable pour l'échelle d'un inventaire perso.
inline std::string generateUuid() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    std::uint64_t a = dist(rng), b = dist(rng);
    // Positionne la version (4) et la variante (10xx) selon la RFC 4122.
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

// Horodatage courant en ISO 8601 UTC (ex. "2026-07-15T09:30:00Z").
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

// Prépare la méta pour une écriture : fixe createdAt au besoin, rafraîchit
// updatedAt et incrémente rev. (L'identité/uuid est portée par l'`id`, attribué
// à part au moment du save.)
inline void stampForSave(SyncMeta& m) {
    const std::string now = nowIso8601Utc();
    if (m.createdAt.empty()) m.createdAt = now;
    m.updatedAt = now;
    ++m.rev;
}

} // namespace domain
