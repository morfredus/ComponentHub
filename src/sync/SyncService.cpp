#include "SyncService.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>

#include "net/HttpClient.h"
#include "sync_meta.h"          // generateUuid
#include "json_tolerant.h"      // lecture JSON sans exception sur champ mal typé
#include "attachment_store.h"   // magasin de blobs local + chutil::sha256Hex

using nlohmann::json;
using domain::SyncRecord;
using domain::jget;

namespace chsync {
namespace {

// SyncRecord -> enveloppe de synchronisation (docs/sync-contract.md §1).
// La charge "data" est l'objet JSON complet de l'entité, tel que stocké.
json toEnvelope(const SyncRecord& r, const std::string& deviceId) {
    return json{
        {"id", r.id}, {"type", r.type}, {"deleted", r.meta.deleted},
        {"rev", r.meta.rev}, {"origin", deviceId},
        {"createdAt", r.meta.createdAt}, {"updatedAt", r.meta.updatedAt},
        {"data", r.data},
    };
}

SyncRecord fromEnvelope(const json& e) {
    SyncRecord r;
    r.id = jget(e, "id", std::string());
    r.type = jget(e, "type", std::string());
    r.meta.deleted = jget(e, "deleted", false);
    r.meta.rev = jget(e, "rev", (std::int64_t)0);
    r.meta.createdAt = jget(e, "createdAt", std::string());
    r.meta.updatedAt = jget(e, "updatedAt", std::string());
    r.data = (e.contains("data") && e["data"].is_object()) ? e["data"] : json::object();
    return r;
}

std::map<std::string, std::string> authHeaders(const SyncConfig& cfg) {
    std::map<std::string, std::string> h;
    if (!cfg.token.empty()) h["Authorization"] = "Bearer " + cfg.token;
    return h;
}

// Pose l'indicateur d'annulation du thread courant pour la durée d'un appel public, même sur
// exception (le client HTTP le lit en thread_local).
struct ThreadCancelScope {
    explicit ThreadCancelScope(const std::atomic<bool>* flag) { chnet::setThreadCancelFlag(flag); }
    ~ThreadCancelScope() { chnet::setThreadCancelFlag(nullptr); }
};

std::string base(const SyncConfig& cfg) {
    std::string u = cfg.serverUrl;
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u;
}

} // namespace

SyncService::SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath,
                         domain::AttachmentStore* attachments)
    : repos_(std::move(repos)), attachments_(attachments), statePath_(std::move(statePath)) {
    for (auto* r : repos_) if (r) byType_[r->syncType()] = r;
    loadState();
}

void SyncService::loadState() {
    std::ifstream in(statePath_);
    if (in) {
        try {
            json j; in >> j;
            deviceId_ = j.value("deviceId", std::string());
            lastSeq_  = j.value("lastSeq", (std::int64_t)0);
            journalId_ = j.value("journalId", std::string());
            if (j.contains("pushWatermarks") && j["pushWatermarks"].is_object())
                for (auto it = j["pushWatermarks"].begin(); it != j["pushWatermarks"].end(); ++it)
                    if (it.value().is_number_integer()) pushWatermarks_[it.key()] = it.value().get<std::int64_t>();
        } catch (...) { /* corrompu : état neuf */ }
    }
    if (deviceId_.empty()) {
        deviceId_ = "device-" + domain::generateUuid();
        saveState();
    }
}

void SyncService::saveState() const {
    json wm = json::object();
    for (const auto& [type, seq] : pushWatermarks_) wm[type] = seq;
    json j{{"deviceId", deviceId_}, {"lastSeq", lastSeq_},
           {"journalId", journalId_}, {"pushWatermarks", wm}};
    // Écriture atomique (fichier temporaire puis renommage) : la sauvegarde complète copie ce
    // fichier depuis le thread de l'interface pendant que la synchro peut le réécrire, et un
    // arrêt brutal ne doit jamais laisser un état tronqué (deviceId perdu = nouvel appareil).
    namespace fs = std::filesystem;
    const fs::path target = fs::u8path(statePath_);
    const fs::path tmp = fs::u8path(statePath_ + ".tmp");
    std::error_code ec;
    {
        std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
        out << j.dump(2);
        out.flush();
        if (!out) { fs::remove(tmp, ec); return; }
    }
    fs::rename(tmp, target, ec);   // remplace l'existant (std::filesystem l'autorise sous Windows)
    if (ec) fs::remove(tmp, ec);
}

void SyncService::progress(SyncProgress::Phase phase, int done, int total) const {
    if (observer_.onProgress) observer_.onProgress(SyncProgress{phase, done, total});
}

chnet::HttpResult SyncService::http(const std::string& method, const std::string& url,
                                    const std::string& body,
                                    const std::map<std::string, std::string>& headers,
                                    int timeoutMs) const {
    if (http_) return http_(method, url, body, headers, timeoutMs);
    if (method == "POST") return chnet::httpPost(url, body, headers, timeoutMs);
    if (method == "PUT")  return chnet::httpPut(url, body, headers, timeoutMs);
    if (method == "HEAD") return chnet::httpHead(url, headers, timeoutMs);
    return chnet::httpGet(url, headers, timeoutMs);
}

bool SyncService::testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs) {
    ThreadCancelScope scope(cancel_);
    auto r = http("GET", base(cfg) + "/healthz", "", authHeaders(cfg), timeoutMs);
    if (!r.ok)            { info = r.error; return false; }
    if (r.status != 200)  { info = "sync.http_status|" + std::to_string(r.status); return false; }
    try {
        json j = json::parse(r.body);
        info = "hub v" + j.value("version", std::string("?")) +
               " (" + j.value("status", std::string("?")) + ")";
    } catch (...) { info = "sync.unexpected_response"; }
    return true;
}

SyncOutcome SyncService::sync(const SyncConfig& cfg) {
    return syncGuarded(cfg, /*mayReset=*/true);
}

// Enveloppe commune : indicateur d'annulation du thread, et exceptions (base occupée, disque
// plein...) converties en erreur lisible plutôt que propagées dans le thread de travail.
SyncOutcome SyncService::syncGuarded(const SyncConfig& cfg, bool mayReset) {
    ThreadCancelScope scope(cancel_);
    SyncOutcome out;
    try {
        out = syncOnce(cfg, mayReset);
    } catch (const std::exception& e) {
        out = SyncOutcome();
        out.error = std::string("sync.database_error|") + e.what();
    } catch (...) {
        out = SyncOutcome();
        out.error = "sync.database_error|unknown";
    }
    if (!out.ok && (isCancelled() || out.error == "http.cancelled")) {
        out.cancelled = true;
        out.error = "sync.cancelled";
    }
    return out;
}

SyncOutcome SyncService::resetHubAndRepublish(const SyncConfig& cfg) {
    ThreadCancelScope scope(cancel_);
    SyncOutcome out;
    const std::string url = base(cfg);
    const auto headers = authHeaders(cfg);

    auto health = http("GET", url + "/healthz", "", headers);
    if (!health.ok)           { out.error = health.error; return out; }
    if (health.status != 200) { out.error = "sync.hub_unreachable|" + std::to_string(health.status); return out; }

    auto reset = http("POST", url + "/api/" + cfg.domain + "/reset", "{}", headers);
    if (!reset.ok)            { out.error = reset.error; return out; }
    if (reset.status == 401)  { out.error = "sync.authentication_refused"; return out; }
    if (reset.status != 200)  { out.error = "sync.reset_refused|" + std::to_string(reset.status); return out; }

    // Le hub vient de créer une nouvelle époque : aucun curseur ni repère de
    // PUSH précédent ne doit empêcher la republication de la copie locale.
    lastSeq_ = 0;
    journalId_.clear();
    pushWatermarks_.clear();
    saveState();
    return syncGuarded(cfg, /*mayReset=*/false);
}

SyncOutcome SyncService::syncOnce(const SyncConfig& cfg, bool mayReset) {
    SyncOutcome out;
    const std::string url = base(cfg);
    const auto headers = authHeaders(cfg);

    // 1) Joignabilité (base locale souveraine : si le hub est absent, on n'échoue
    //    pas silencieusement - l'appelant décide, l'appli continue en local).
    progress(SyncProgress::Phase::Connect);
    auto health = http("GET", url + "/healthz", "", headers);
    if (!health.ok)           { out.error = health.error; return out; }
    if (health.status != 200) { out.error = "sync.hub_unreachable|" + std::to_string(health.status); return out; }
    if (isCancelled()) return out;

    const std::string changesUrl = url + "/api/" + cfg.domain + "/changes";

    // 2) PUSH INCRÉMENTAL : seulement les entités modifiées localement depuis le
    //    dernier envoi (repère de push par table). Rien de changé -> aucun envoi.
    {
        json changes = json::array();
        std::map<std::string, std::int64_t> newWatermarks;
        for (auto* repo : repos_) {
            const std::string type = repo->syncType();
            // Aucun repère encore -> première synchro de cette table : on envoie
            // tout (since=-1 inclut les entités héritées à localSeq=0).
            std::int64_t since = -1;
            if (auto it = pushWatermarks_.find(type); it != pushWatermarks_.end()) since = it->second;

            // Le repère de cet envoi est lu AVANT l'export : l'utilisateur continue d'écrire
            // pendant la synchro (autre connexion). Une ligne modifiée après cette lecture a une
            // séquence supérieure au repère et repart donc au push suivant ; lu APRÈS l'export,
            // le repère la dépasserait sans qu'elle ait été envoyée (modification perdue).
            const std::int64_t maxSeq = repo->maxLocalSeq();

            // Le passage JSON -> SQLite repart avec des séquences locales à 0.
            // Un repère conservé dans sync_state.json peut donc être supérieur à
            // la base courante et masquer toutes les lignes. On le rend alors
            // invalide pour republier la table une seule fois.
            if (maxSeq < since)
                since = -1;
            for (const auto& rec : repo->exportForSync(since))
                changes.push_back(toEnvelope(rec, deviceId_));
            newWatermarks[type] = maxSeq;  // repère à valider si l'envoi réussit
        }
        out.accepted = static_cast<int>(changes.size());  // « envoyés »
        if (isCancelled()) return out;

        if (!changes.empty()) {
            progress(SyncProgress::Phase::Push, 0, out.accepted);
            json body{{"deviceId", deviceId_}, {"changes", changes}};
            auto pr = http("POST", changesUrl, body.dump(), headers);
            if (!pr.ok)            { out.error = pr.error; return out; }
            if (pr.status == 401)  { out.error = "sync.authentication_refused"; return out; }
            if (pr.status != 200)  { out.error = "sync.push_failed|" + std::to_string(pr.status); return out; }
            try {
                json j = json::parse(pr.body);
                out.conflicts = j.contains("conflicts") ? (int)j["conflicts"].size() : 0;
            } catch (...) { out.error = "sync.push_invalid_response"; return out; }
        }
        // Envoi réussi (ou rien à envoyer) : on avance les repères et on persiste
        // immédiatement (évite de tout re-pousser si le PULL échoue ensuite).
        for (const auto& [type, seq] : newWatermarks) pushWatermarks_[type] = seq;
        saveState();
    }

    // 3) PULL depuis le curseur, avec pagination et dispatch par type.
    std::int64_t since = lastSeq_;
    bool firstPull = true;
    for (;;) {
        if (isCancelled()) return out;
        progress(SyncProgress::Phase::Pull, out.applied + out.rejected, 0);
        auto gr = http("GET", changesUrl + "?since=" + std::to_string(since) + "&limit=500", "", headers);
        if (!gr.ok)           { out.error = gr.error; return out; }
        if (gr.status != 200) { out.error = "sync.pull_failed|" + std::to_string(gr.status); return out; }

        json j;
        try { j = json::parse(gr.body); } catch (...) { j = json(); }
        if (!j.is_object())   { out.error = "sync.pull_invalid_response"; return out; }

        // Détection d'époque : si le journal du hub a changé (dossier déplacé /
        // réinitialisé), notre curseur et nos repères de push ne valent plus.
        // On réinitialise et on relance une synchro complète, une seule fois.
        if (firstPull) {
            firstPull = false;
            const std::string remoteJournal = jget(j, "journalId", std::string());
            if (mayReset && !journalId_.empty() && !remoteJournal.empty()
                && remoteJournal != journalId_) {
                journalId_ = remoteJournal;
                lastSeq_ = 0;
                pushWatermarks_.clear();
                saveState();
                return syncOnce(cfg, /*mayReset=*/false);  // full push + full pull
            }
            if (!remoteJournal.empty()) journalId_ = remoteJournal;
        }

        // Toute la page est appliquée dans UNE transaction (un seul fsync, tout ou rien si le
        // disque lâche) ; BEGIN IMMEDIATE verrouille l'écriture d'emblée pour que « révision
        // distante plus récente ? » et l'écriture ne soient pas séparées par une écriture de
        // l'utilisateur. Chaque enregistrement a en plus son propre try/catch : un enregistrement
        // mal typé est compté et journalisé, il ne bloque pas la synchro, et le curseur avance
        // quand même (sinon la même page serait rejouée à vie).
        if (sqlExec_) sqlExec_("BEGIN IMMEDIATE");
        try {
            const json changes = j.contains("changes") && j["changes"].is_array()
                                     ? j["changes"] : json::array();
            for (const auto& ch : changes) {
                std::string id = ch.is_object() ? jget(ch, "id", std::string()) : std::string();
                try {
                    SyncRecord rec = fromEnvelope(ch);
                    auto it = byType_.find(rec.type);
                    if (it != byType_.end() && it->second->applyRemote(rec)) out.applied++;
                    // Type inconnu (table pas encore gérée par ce client) : ignoré.
                } catch (const std::exception& ex) {
                    out.rejected++;
                    std::fprintf(stderr, "[sync] enregistrement distant rejeté (%s) : %s\n",
                                 id.empty() ? "?" : id.c_str(), ex.what());
                    if (observer_.onReject) observer_.onReject(id, ex.what());
                } catch (...) {
                    out.rejected++;
                    if (observer_.onReject) observer_.onReject(id, "unknown");
                }
            }
            if (sqlExec_) sqlExec_("COMMIT");
        } catch (...) {
            if (sqlExec_) { try { sqlExec_("ROLLBACK"); } catch (...) {} }
            throw;   // syncGuarded : "sync.database_error"
        }
        const std::int64_t nextSince = jget(j, "lastSeq", since);
        const bool hasMore = jget(j, "hasMore", false);
        // Un hub qui annonce « encore » sans avancer le curseur nous ferait boucler à l'infini.
        if (hasMore && nextSince <= since) { out.error = "sync.pull_invalid_response"; return out; }
        since = nextSince;

        // Curseur persisté page par page : une annulation ou une coupure reprend à la page
        // suivante au lieu de rejouer toute la synchro (l'application est idempotente de toute façon).
        lastSeq_ = since;
        saveState();

        if (!hasMore) break;
    }

    out.lastSeq = lastSeq_;

    // 4) Transfert du binaire des pièces jointes. Les métadonnées sont déjà
    //    synchronisées : le transfert des blobs est best-effort et ne fait pas
    //    échouer la synchro s'il achoppe (un blob manquant se rattrape à la passe
    //    suivante, la référence de hash restant valide).
    transferBlobs(cfg, out);

    if (isCancelled()) return out;
    out.ok = true;
    return out;
}

void SyncService::transferBlobs(const SyncConfig& cfg, SyncOutcome& out) {
    if (!attachments_) return;

    // Ensemble dédupliqué des hashes référencés par toutes les tables (en pratique
    // seuls les documents en portent).
    std::set<std::string> hashes;
    for (auto* repo : repos_)
        for (const auto& h : repo->referencedBlobs())
            if (domain::AttachmentStore::validHash(h)) hashes.insert(h);
    if (hashes.empty()) return;

    const std::string url = base(cfg);
    const auto headers = authHeaders(cfg);

    int done = 0;
    const int total = static_cast<int>(hashes.size());
    for (const auto& h : hashes) {
        if (isCancelled()) return;   // annulation coopérative entre deux blobs
        progress(SyncProgress::Phase::Blobs, done++, total);
        const std::string blobUrl = url + "/api/blob/" + h;

        if (attachments_->has(h)) {
            // Contenu présent localement : le téléverser SI le hub ne l'a pas déjà.
            // Le HEAD évite de renvoyer un binaire déjà stocké (adressage par
            // contenu = un seul exemplaire par hash).
            auto head = http("HEAD", blobUrl, "", headers);
            if (!head.ok) continue;            // hub injoignable pour les blobs : best-effort
            if (head.status == 200) continue;  // déjà présent côté hub
            if (head.status == 404) {
                std::string bytes;
                if (!attachments_->get(h, bytes)) continue;
                auto put = http("PUT", blobUrl, bytes, headers);
                if (put.ok && (put.status == 201 || put.status == 200)) out.blobsUploaded++;
            }
            // 405 (hub sans magasin de blobs) ou autre : on ignore proprement,
            // la synchro des métadonnées a déjà réussi.
        } else {
            // Contenu absent localement : le télécharger, en VÉRIFIANT l'intégrité
            // (on re-hache ce qu'on reçoit ; le hub ne garantit pas le contenu).
            auto get = http("GET", blobUrl, "", headers);
            if (!get.ok || get.status != 200) continue;      // pas encore dispo : prochaine passe
            if (chutil::sha256Hex(get.body) != h) continue;  // corrompu : on refuse
            if (attachments_->put(h, get.body)) out.blobsDownloaded++;
        }
    }
}

} // namespace chsync
