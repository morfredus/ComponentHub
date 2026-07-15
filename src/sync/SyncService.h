/**
 * SyncService — client de synchronisation de ComponentHub vers HomeServerHub.
 *
 * Générique : opère sur une liste de dépôts synchronisables
 * (domain::ISyncableRepository), donc sur TOUTES les tables (composants,
 * emplacements, catégories, projets, nomenclatures, référentiels, documents)
 * sans connaître leur métier. PUSH de l'état local, puis PULL depuis un curseur
 * (lastSeq) avec dispatch par type. Respecte docs/sync-contract.md.
 *
 * Base locale souveraine : la synchro n'est jamais obligatoire ; en cas de hub
 * injoignable, l'application continue sur sa copie locale.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "sync_record.h"

namespace chsync {

struct SyncConfig {
    std::string serverUrl;               // ex. http://homeserverhub.local:8080
    std::string token;                   // optionnel (Bearer) ; vide = pas d'auth
    std::string domain = "componenthub"; // journal côté hub (toutes les tables)
};

struct SyncOutcome {
    bool ok = false;
    std::string error;
    int accepted = 0;         // changements locaux acceptés par le hub (PUSH)
    int conflicts = 0;        // versions refusées par le hub (plus anciennes)
    int applied = 0;          // changements distants appliqués localement (PULL)
    std::int64_t lastSeq = 0; // curseur après synchro
};

class SyncService {
public:
    // `repos` : tous les dépôts à synchroniser (leur durée de vie doit couvrir
    // celle du SyncService). `statePath` : fichier local deviceId + curseur.
    SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath);

    // Teste la joignabilité du hub via GET /api/health. `info` reçoit un résumé
    // lisible en cas de succès, le message d'erreur sinon.
    bool testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs = 0);

    // Cycle complet : PUSH de l'état local puis PULL depuis le curseur.
    SyncOutcome sync(const SyncConfig& cfg);

    const std::string& deviceId() const { return deviceId_; }
    std::int64_t lastSeq() const { return lastSeq_; }

private:
    void loadState();
    void saveState() const;

    std::vector<domain::ISyncableRepository*> repos_;
    std::map<std::string, domain::ISyncableRepository*> byType_; // dispatch au PULL
    std::string statePath_;
    std::string deviceId_;
    std::int64_t lastSeq_ = 0;                                   // curseur de PULL
    std::map<std::string, std::int64_t> pushWatermarks_;         // repère de PUSH par table
};

} // namespace chsync
