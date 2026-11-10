#include "SyncService.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>

#include "net/HttpClient.h"
#include "sync_meta.h"          // generateUuid
#include "json_tolerant.h"      // JSON reading that does not throw on a badly typed field
#include "attachment_store.h"   // magasin de blobs local + chutil::sha256Hex

using nlohmann::json;
using domain::SyncRecord;
using domain::jget;

namespace chsync {
namespace {

// SyncRecord -> synchronization envelope (docs/sync-contract.md §1).
// The "data" payload is the complete JSON object of the entity, as stored.
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

// Sets the cancellation flag of the current thread for the duration of a public call, even on
// exception (the HTTP client reads it as thread_local).
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
    // Atomic write (temporary file then rename): the full backup copies this
    // file from the interface thread while synchronization may rewrite it, and an
    // abrupt stop must never leave a truncated state (lost deviceId = new device).
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

// Common wrapper: thread cancellation flag, and exceptions (busy database, full
// disk...) converted to a readable error instead of propagating in the worker thread.
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

    // The hub has just created a new epoch: no previous PUSH cursor or marker
    // must prevent republishing the local copy.
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

    // 1) Reachability (sovereign local database: if the hub is absent, we do not fail
    //    silently - the caller decides, the app continues locally).
    progress(SyncProgress::Phase::Connect);
    auto health = http("GET", url + "/healthz", "", headers);
    if (!health.ok)           { out.error = health.error; return out; }
    if (health.status != 200) { out.error = "sync.hub_unreachable|" + std::to_string(health.status); return out; }
    if (isCancelled()) return out;

    const std::string changesUrl = url + "/api/" + cfg.domain + "/changes";

    // 2) INCREMENTAL PUSH: only the entities changed locally since the
    //    last upload (push marker per table). Nothing changed -> nothing sent.
    {
        json changes = json::array();
        std::map<std::string, std::int64_t> newWatermarks;
        for (auto* repo : repos_) {
            const std::string type = repo->syncType();
            // No marker yet -> first synchronization of this table: send
            // everything (since=-1 includes legacy entities with localSeq=0).
            std::int64_t since = -1;
            if (auto it = pushWatermarks_.find(type); it != pushWatermarks_.end()) since = it->second;

            // The marker of this upload is read BEFORE the export: the user keeps writing
            // during synchronization (another connection). A row modified after this read has a
            // sequence greater than the marker and therefore goes out at the next push; read AFTER the export,
            // the marker would skip it without it having been sent (lost modification).
            const std::int64_t maxSeq = repo->maxLocalSeq();

            // The JSON -> SQLite conversion restarts with local sequences at 0.
            // A marker kept in sync_state.json can therefore be greater than
            // the current database and hide all rows. It is then made
            // invalid to republish the table once.
            if (maxSeq < since)
                since = -1;
            for (const auto& rec : repo->exportForSync(since))
                changes.push_back(toEnvelope(rec, deviceId_));
            newWatermarks[type] = maxSeq;  // marker to validate if the upload succeeds
        }
        out.accepted = static_cast<int>(changes.size());  // "sent"
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
        // Upload succeeded (or nothing to send): advance the markers and persist them
        // immediately (avoids pushing everything again if the PULL fails afterwards).
        for (const auto& [type, seq] : newWatermarks) pushWatermarks_[type] = seq;
        saveState();
    }

    // 3) PULL from the cursor, with pagination and dispatch by type.
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

        // Epoch detection: if the hub's journal changed (folder moved /
        // reset), our cursor and push markers are no longer valid.
        // We reset and restart a full synchronization, only once.
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

        // The whole page is applied in ONE transaction (a single fsync, all or nothing if the
        // disk fails); BEGIN IMMEDIATE takes the write lock right away so that "is the remote
        // revision newer?" and the write are not separated by a user write.
        // Each record also has its own try/catch: a badly typed record
        // is counted and logged, it does not block synchronization, and the cursor still advances
        // (otherwise the same page would be replayed forever).
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
                    // Unknown type (table not handled by this client yet): ignored.
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
        // A hub that announces "more" without advancing the cursor would make us loop forever.
        if (hasMore && nextSince <= since) { out.error = "sync.pull_invalid_response"; return out; }
        since = nextSince;

        // Cursor persisted page by page: a cancellation or a disconnection resumes at the
        // next page instead of replaying the whole synchronization (applying is idempotent anyway).
        lastSeq_ = since;
        saveState();

        if (!hasMore) break;
    }

    out.lastSeq = lastSeq_;

    // 4) Transfer of the attachment binaries. The metadata is already
    //    synchronized: the blob transfer is best-effort and does not make
    //    the synchronization fail if it stumbles (a missing blob is caught up on the next
    //    pass, the hash reference staying valid).
    transferBlobs(cfg, out);

    if (isCancelled()) return out;
    out.ok = true;
    return out;
}

void SyncService::transferBlobs(const SyncConfig& cfg, SyncOutcome& out) {
    if (!attachments_) return;

    // Deduplicated set of the hashes referenced by all tables (in practice
    // only documents carry some).
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
        if (isCancelled()) return;   // cooperative cancellation between two blobs
        progress(SyncProgress::Phase::Blobs, done++, total);
        const std::string blobUrl = url + "/api/blob/" + h;

        if (attachments_->has(h)) {
            // Content present locally: upload it IF the hub does not already have it.
            // HEAD avoids sending again a binary that is already stored (content
            // addressing = a single copy per hash).
            auto head = http("HEAD", blobUrl, "", headers);
            if (!head.ok) continue;            // hub unreachable for blobs: best-effort
            if (head.status == 200) continue;  // already present on the hub side
            if (head.status == 404) {
                std::string bytes;
                if (!attachments_->get(h, bytes)) continue;
                auto put = http("PUT", blobUrl, bytes, headers);
                if (put.ok && (put.status == 201 || put.status == 200)) out.blobsUploaded++;
            }
            // 405 (hub without a blob store) or other: ignored cleanly,
            // the metadata synchronization already succeeded.
        } else {
            // Content missing locally: download it, CHECKING integrity
            // (we re-hash what we receive; the hub does not guarantee the content).
            auto get = http("GET", blobUrl, "", headers);
            if (!get.ok || get.status != 200) continue;      // not available yet: next pass
            if (chutil::sha256Hex(get.body) != h) continue;  // corrompu : on refuse
            if (attachments_->put(h, get.body)) out.blobsDownloaded++;
        }
    }
}

} // namespace chsync
