/**
 * SyncService — client de synchronisation de ComponentHub vers morfSync.
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

namespace domain { class AttachmentStore; }

namespace chsync {

struct SyncConfig {
    std::string serverUrl;               // ex. http://morfsync.local:8080
    std::string token;                   // optionnel (Bearer) ; vide = pas d'auth
    std::string domain = "componenthub"; // journal côté hub (toutes les tables)
};

struct SyncOutcome {
    bool ok = false;
    std::string error;
    int accepted = 0;         // changements locaux acceptés par le hub (PUSH)
    int conflicts = 0;        // versions refusées par le hub (plus anciennes)
    int applied = 0;          // changements distants appliqués localement (PULL)
    int blobsUploaded = 0;    // binaires de pièces jointes envoyés au hub
    int blobsDownloaded = 0;  // binaires de pièces jointes récupérés du hub
    std::int64_t lastSeq = 0; // curseur après synchro
};

class SyncService {
public:
    // `repos` : tous les dépôts à synchroniser (leur durée de vie doit couvrir
    // celle du SyncService). `statePath` : fichier local deviceId + curseur.
    // `attachments` (facultatif) : magasin local des pièces jointes ; s'il est
    // fourni, la synchro transfère aussi le BINAIRE des blobs référencés. Nul =
    // synchro des métadonnées uniquement (comportement antérieur).
    SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath,
                domain::AttachmentStore* attachments = nullptr);

    // Teste la joignabilité du hub via GET /api/health. `info` reçoit un résumé
    // lisible en cas de succès, le message d'erreur sinon.
    bool testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs = 0);

    // Cycle complet : PUSH de l'état local puis PULL depuis le curseur.
    // Détecte un changement d'« époque » du journal du hub (dossier déplacé /
    // réinitialisé) et remet le curseur à zéro automatiquement le cas échéant.
    SyncOutcome sync(const SyncConfig& cfg);

    const std::string& deviceId() const { return deviceId_; }
    std::int64_t lastSeq() const { return lastSeq_; }

private:
    void loadState();
    void saveState() const;
    SyncOutcome syncOnce(const SyncConfig& cfg, bool mayReset);
    // Transfère le binaire des blobs référencés (best-effort, après la synchro
    // des métadonnées) : téléverse ceux que le hub n'a pas, télécharge ceux qui
    // manquent localement (intégrité vérifiée). Sans effet si aucun magasin.
    void transferBlobs(const SyncConfig& cfg, SyncOutcome& out);

    std::vector<domain::ISyncableRepository*> repos_;
    domain::AttachmentStore* attachments_ = nullptr;
    std::map<std::string, domain::ISyncableRepository*> byType_; // dispatch au PULL
    std::string statePath_;
    std::string deviceId_;
    std::int64_t lastSeq_ = 0;                                   // curseur de PULL
    std::map<std::string, std::int64_t> pushWatermarks_;         // repère de PUSH par table
    std::string journalId_;                                      // époque du journal hub connue
};

} // namespace chsync
