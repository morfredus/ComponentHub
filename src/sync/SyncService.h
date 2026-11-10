/**
 * SyncService - synchronization client from ComponentHub to morfSync.
 *
 * Generic: operates on a list of synchronizable repositories
 * (domain::ISyncableRepository), hence on ALL tables (components,
 * locations, categories, projects, bills of materials, reference lists, documents)
 * without knowing their business meaning. PUSH of the local state, then PULL from a cursor
 * (lastSeq) with dispatch by type. Follows docs/sync-contract.md.
 *
 * Sovereign local database: synchronization is never mandatory; if the hub is
 * unreachable, the application continues on its local copy.
 *
 * THREADS: an instance (and its repositories / store) belongs to ONE thread only, the one that
 * calls sync(). The application runs it in a dedicated worker thread with its own
 * SQLite connection (see SyncSession). The service touches no interface object: it
 * notifies through SyncObserver, called in the worker thread (the caller bridges it to
 * the GUI thread), and stops cleanly when the cancellation flag turns true.
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
    std::string token;                   // optional (Bearer); empty = no auth
    std::string domain = "componenthub"; // journal on the hub side (all tables)
};

struct SyncOutcome {
    bool ok = false;
    std::string error;
    int accepted = 0;         // local changes accepted by the hub (PUSH)
    int conflicts = 0;        // versions refused by the hub (older)
    int applied = 0;          // remote changes applied locally (PULL)
    int rejected = 0;         // unreadable remote records, ignored (PULL)
    int blobsUploaded = 0;    // attachment binaries sent to the hub
    int blobsDownloaded = 0;  // attachment binaries fetched from the hub
    std::int64_t lastSeq = 0; // cursor after synchronization
    bool cancelled = false;   // stopped on request (error = "sync.cancelled")
};

// Progress, for display. `done`/`total` are 0 when the phase has no denominator.
struct SyncProgress {
    enum class Phase { Connect, Push, Pull, Blobs };
    Phase phase = Phase::Connect;
    int done = 0;
    int total = 0;
};

// Callbacks invoked in the thread that runs sync(). All optional.
struct SyncObserver {
    std::function<void(const SyncProgress&)> onProgress;
    // Unreadable remote record, ignored: identifier (if any) and reason.
    std::function<void(const std::string& id, const std::string& reason)> onReject;
};

// Injectable HTTP transport (tests: fake server). By default, the real chnet::http* client.
using HttpFn = std::function<chnet::HttpResult(
    const std::string& method, const std::string& url, const std::string& body,
    const std::map<std::string, std::string>& headers, int timeoutMs)>;

class SyncService {
public:
    // `repos`: all the repositories to synchronize (their lifetime must cover
    // that of the SyncService). `statePath`: local file with deviceId + cursor.
    // `attachments` (optional): local attachment store; if
    // provided, synchronization also transfers the BINARY of the referenced blobs. Null =
    // metadata-only synchronization (previous behavior).
    SyncService(std::vector<domain::ISyncableRepository*> repos, std::string statePath,
                domain::AttachmentStore* attachments = nullptr);

    // Tests reachability of the hub through GET /healthz. `info` receives a readable
    // summary on success, the error message otherwise.
    bool testConnection(const SyncConfig& cfg, std::string& info, int timeoutMs = 0);

    // Full cycle: PUSH of the local state then PULL from the cursor.
    // Detects a change of the hub journal "epoch" (folder moved /
    // reset) and automatically resets the cursor to zero when needed.
    SyncOutcome sync(const SyncConfig& cfg);

    // Resets only the ComponentHub domain on the hub, forgets the local
    // markers then republishes this local copy in full. Destructive action,
    // to be reserved for an explicit confirmation in the interface.
    SyncOutcome resetHubAndRepublish(const SyncConfig& cfg);

    // SQL executor (BEGIN IMMEDIATE / COMMIT / ROLLBACK) to apply each PULL page in
    // one transaction. Optional: without it, each record is applied on its own.
    // BEGIN IMMEDIATE takes the write lock right away: the check "is the remote revision
    // newer?" and the write are then atomic against user writes.
    void setSqlExec(std::function<void(const char*)> fn) { sqlExec_ = std::move(fn); }

    // Cooperative cancellation: tested between pages, blobs and during HTTP requests
    // (by the calling thread). The flag must outlive the service. nullptr = never cancelled.
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
    // Transfers the binary of the referenced blobs (best-effort, after the
    // metadata synchronization): uploads those the hub lacks, downloads those
    // missing locally (integrity checked). No effect without a store.
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
    std::map<std::string, std::int64_t> pushWatermarks_;         // PUSH marker per table
    std::string journalId_;                                      // known epoch of the hub journal
};

} // namespace chsync
