/**
 * SyncService - client de synchronisation de ComponentHub vers morfSync.
 *
 * Générique : opère sur une liste de dépôts synchronisables
 * (domain::ISyncableRepository), donc sur TOUTES les tables (composants,
 * emplacements, catégories, projets, nomenclatures, référentiels, documents)
 * sans connaître leur métier. PUSH de l'état local, puis PULL depuis un curseur
 * (lastSeq) avec dispatch par type. Respecte docs/sync-contract.md.
 *
 * Base locale souveraine : la synchro n'est jamais obligatoire ; en cas de hub
 * injoignable, l'application continue sur sa copie locale.
 *
 * THREADS : une instance (et ses dépôts / son magasin) appartient à UN seul thread, celui qui
 * appelle sync(). L'application la fait tourner dans un thread de travail dédié avec sa propre
 * connexion SQLite (voir SyncSession). Le service ne touche à aucun objet d'interface : il
 * notifie via SyncObserver, appelé dans le thread de travail (à l'appelant de le ponter vers
 * le thread GUI), et s'arrête proprement quand l'indicateur d'annulation passe à vrai.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <cstdint>
#include <functional>
#include "net/HttpClient.h"
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
    int rejected = 0;         // enregistrements distants illisibles, ignorés (PULL)
    int blobsUploaded = 0;    // binaires de pièces jointes envoyés au hub
    int blobsDownloaded = 0;  // binaires de pièces jointes récupérés du hub
    std::int64_t lastSeq = 0; // curseur après synchro
    bool cancelled = false;   // arrêtée sur demande (error = "sync.cancelled")
};

// Avancement, pour l'affichage. `done`/`total` valent 0 quand la phase n'a pas de dénominateur.
struct SyncProgress {
    enum class Phase { Connect, Push, Pull, Blobs };
    Phase phase = Phase::Connect;
    int done = 0;
    int total = 0;
};

// Rappels appelés dans le thread qui exécute sync(). Tous facultatifs.
struct SyncObserver {
    std::function<void(const SyncProgress&)> onProgress;
    // Enregistrement distant illisible, ignoré : identifiant (s'il existe) et motif.
    std::function<void(const std::string& id, const std::string& reason)> onReject;
};

// Transport HTTP injectable (tests : faux serveur). Par défaut, le client réel chnet::http*.
using HttpFn = std::function<chnet::HttpResult(
    const std::string& method, const std::string& url, const std::string& body,
    const std::map<std::string, std::string>& headers, int timeoutMs)>;

class SyncService {
public:
    // `repos` : tous les dépôts à synchroniser (leur durée de vie doit couvrir
    // celle du SyncService). `statePath` : fichier local deviceId + curseur.
    // `attachments` (facultatif) : magasin local des pièces jointes ; s'il est
    // fourni, la synchro transfère aussi le BINAIRE des blobs référencés. Nul =
    // synchro des métadonnées uniquement (comportement antérieur).
    SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath,
                domain::AttachmentStore* attachments = nullptr);

    // Teste la joignabilité du hub via GET /healthz. `info` reçoit un résumé
    // lisible en cas de succès, le message d'erreur sinon.
    bool testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs = 0);

    // Cycle complet : PUSH de l'état local puis PULL depuis le curseur.
    // Détecte un changement d'« époque » du journal du hub (dossier déplacé /
    // réinitialisé) et remet le curseur à zéro automatiquement le cas échéant.
    SyncOutcome sync(const SyncConfig& cfg);

    // Réinitialise le seul domaine ComponentHub sur le hub, oublie les repères
    // locaux puis republie intégralement cette copie locale. Action destructive
    // à réserver à une confirmation explicite dans l'interface.
    SyncOutcome resetHubAndRepublish(const SyncConfig& cfg);

    // Exécuteur SQL (BEGIN IMMEDIATE / COMMIT / ROLLBACK) pour appliquer chaque page du PULL dans
    // une transaction. Facultatif : sans lui, chaque enregistrement est appliqué isolément.
    // BEGIN IMMEDIATE prend le verrou d'écriture d'emblée : la vérification « révision distante
    // plus récente ? » et l'écriture sont alors atomiques face aux écritures de l'utilisateur.
    void setSqlExec(std::function<void(const char*)> fn) { sqlExec_ = std::move(fn); }

    // Annulation coopérative : testée entre les pages, les blobs et pendant les requêtes HTTP
    // (par le thread appelant). L'indicateur doit survivre au service. nullptr = jamais annulée.
    void setCancelFlag(const std::atomic<bool>* flag) { cancel_ = flag; }
    void setObserver(SyncObserver obs) { observer_ = std::move(obs); }
    void setHttp(HttpFn fn) { http_ = std::move(fn); }

    const std::string& deviceId() const { return deviceId_; }
    std::int64_t lastSeq() const { return lastSeq_; }

private:
    bool isCancelled() const { return cancel_ && cancel_->load(std::memory_order_relaxed); }
    void progress(SyncProgress::Phase phase, int done = 0, int total = 0) const;
    chnet::HttpResult http(const std::string& method, const std::string& url, const std::string& body,
                           const std::map<std::string, std::string>& headers, int timeoutMs = 0) const;
    SyncOutcome syncGuarded(const SyncConfig& cfg, bool mayReset);
    void loadState();
    void saveState() const;
    SyncOutcome syncOnce(const SyncConfig& cfg, bool mayReset);
    // Transfère le binaire des blobs référencés (best-effort, après la synchro
    // des métadonnées) : téléverse ceux que le hub n'a pas, télécharge ceux qui
    // manquent localement (intégrité vérifiée). Sans effet si aucun magasin.
    void transferBlobs(const SyncConfig& cfg, SyncOutcome& out);

    std::function<void(const char*)> sqlExec_;
    const std::atomic<bool>* cancel_ = nullptr;
    SyncObserver observer_;
    HttpFn http_;
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
