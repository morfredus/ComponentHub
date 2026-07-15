#include "SyncService.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "net/HttpClient.h"
#include "sync_meta.h"   // generateUuid

using nlohmann::json;
using domain::SyncRecord;

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
    r.id = e.value("id", std::string());
    r.type = e.value("type", std::string());
    r.meta.deleted = e.value("deleted", false);
    r.meta.rev = e.value("rev", (std::int64_t)0);
    r.meta.createdAt = e.value("createdAt", std::string());
    r.meta.updatedAt = e.value("updatedAt", std::string());
    r.data = (e.contains("data") && e["data"].is_object()) ? e["data"] : json::object();
    return r;
}

std::map<std::string, std::string> authHeaders(const SyncConfig& cfg) {
    std::map<std::string, std::string> h;
    if (!cfg.token.empty()) h["Authorization"] = "Bearer " + cfg.token;
    return h;
}
std::string base(const SyncConfig& cfg) {
    std::string u = cfg.serverUrl;
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u;
}

} // namespace

SyncService::SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath)
    : repos_(std::move(repos)), statePath_(std::move(statePath)) {
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
                    pushWatermarks_[it.key()] = it.value().get<std::int64_t>();
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
    std::ofstream out(statePath_, std::ios::trunc);
    out << j.dump(2);
}

bool SyncService::testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs) {
    auto r = chnet::httpGet(base(cfg) + "/api/health", authHeaders(cfg), timeoutMs);
    if (!r.ok)            { info = r.error; return false; }
    if (r.status != 200)  { info = "HTTP " + std::to_string(r.status); return false; }
    try {
        json j = json::parse(r.body);
        info = "hub v" + j.value("version", std::string("?")) +
               " (" + j.value("status", std::string("?")) + ")";
    } catch (...) { info = "réponse inattendue"; }
    return true;
}

SyncOutcome SyncService::sync(const SyncConfig& cfg) {
    return syncOnce(cfg, /*mayReset=*/true);
}

SyncOutcome SyncService::syncOnce(const SyncConfig& cfg, bool mayReset) {
    SyncOutcome out;
    const std::string url = base(cfg);
    const auto headers = authHeaders(cfg);

    // 1) Joignabilité (base locale souveraine : si le hub est absent, on n'échoue
    //    pas silencieusement — l'appelant décide, l'appli continue en local).
    auto health = chnet::httpGet(url + "/api/health", headers);
    if (!health.ok)           { out.error = health.error; return out; }
    if (health.status != 200) { out.error = "hub injoignable (HTTP " + std::to_string(health.status) + ")"; return out; }

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
            for (const auto& rec : repo->exportForSync(since))
                changes.push_back(toEnvelope(rec, deviceId_));
            newWatermarks[type] = repo->maxLocalSeq();  // repère à valider si l'envoi réussit
        }
        out.accepted = static_cast<int>(changes.size());  // « envoyés »

        if (!changes.empty()) {
            json body{{"deviceId", deviceId_}, {"changes", changes}};
            auto pr = chnet::httpPost(changesUrl, body.dump(), headers);
            if (!pr.ok)            { out.error = pr.error; return out; }
            if (pr.status == 401)  { out.error = "authentification refusée (jeton ?)"; return out; }
            if (pr.status != 200)  { out.error = "PUSH a échoué (HTTP " + std::to_string(pr.status) + ")"; return out; }
            try {
                json j = json::parse(pr.body);
                out.conflicts = j.contains("conflicts") ? (int)j["conflicts"].size() : 0;
            } catch (...) { out.error = "réponse PUSH illisible"; return out; }
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
        auto gr = chnet::httpGet(changesUrl + "?since=" + std::to_string(since) + "&limit=500", headers);
        if (!gr.ok)           { out.error = gr.error; return out; }
        if (gr.status != 200) { out.error = "PULL a échoué (HTTP " + std::to_string(gr.status) + ")"; return out; }

        bool hasMore = false;
        try {
            json j = json::parse(gr.body);

            // Détection d'époque : si le journal du hub a changé (dossier déplacé /
            // réinitialisé), notre curseur et nos repères de push ne valent plus.
            // On réinitialise et on relance une synchro complète, une seule fois.
            if (firstPull) {
                firstPull = false;
                const std::string remoteJournal = j.value("journalId", std::string());
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

            for (const auto& ch : j.value("changes", json::array())) {
                SyncRecord rec = fromEnvelope(ch);
                auto it = byType_.find(rec.type);
                if (it != byType_.end() && it->second->applyRemote(rec)) out.applied++;
                // Type inconnu (table pas encore gérée par ce client) : ignoré.
            }
            since   = j.value("lastSeq", since);
            hasMore = j.value("hasMore", false);
        } catch (...) { out.error = "réponse PULL illisible"; return out; }

        if (!hasMore) break;
    }

    lastSeq_ = since;
    saveState();
    out.lastSeq = lastSeq_;
    out.ok = true;
    return out;
}

} // namespace chsync
