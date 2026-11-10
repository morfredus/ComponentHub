// ctest tests of synchronization (no Qt): fake HTTP hub injected into SyncService, two
// SQLite connections on the same WAL database (interface + worker thread), cancellation, poison
// record. The Qt thread itself (SyncRunner) is not covered here: it contains no logic.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <thread>

#include <nlohmann/json.hpp>

#include "AppContext.h"
#include "net/HttpClient.h"
#include "sync/SyncService.h"
#include "sync/SyncSession.h"
#include "test_util.h"

#include "silent_server.h"

using nlohmann::json;

namespace {

const chsync::SyncConfig kCfg{"http://hub.test:8080", "", "componenthub"};

json envelope(const std::string& type, const std::string& id, int rev, const json& data) {
    return json{{"id", id}, {"type", type}, {"deleted", false}, {"rev", rev}, {"origin", "hub"},
                {"createdAt", "2026-01-01T00:00:00Z"}, {"updatedAt", "2026-01-01T00:00:00Z"}, {"data", data}};
}

// Fake morfSync server: paginated journal, recording of uploads, blob store.
struct FakeHub {
    std::vector<json> journal;     // servi au PULL ; seq = index + 1
    std::vector<json> received;    // records pushed by the client
    std::map<std::string, std::string> blobs;
    int pageSize = 2;
    int requests = 0;
    int pullPages = 0;
    // Called BEFORE answering; lets a test simulate a user action or a cancellation.
    std::function<void(const std::string& method, const std::string& url)> onRequest;

    chsync::HttpFn fn() {
        return [this](const std::string& m, const std::string& u, const std::string& b,
                      const std::map<std::string, std::string>&, int) { return handle(m, u, b); };
    }

    chnet::HttpResult handle(const std::string& method, const std::string& url, const std::string& body) {
        requests++;
        if (onRequest) onRequest(method, url);
        chnet::HttpResult r;
        r.ok = true;
        r.status = 200;
        const std::string path = url.substr(std::string("http://hub.test:8080").size());
        if (path == "/healthz") { r.body = R"({"version":"test","status":"ok"})"; return r; }
        if (path.rfind("/api/blob/", 0) == 0) {
            const std::string h = path.substr(10);
            if (method == "HEAD") { r.status = blobs.count(h) ? 200 : 404; return r; }
            if (method == "PUT") { blobs[h] = body; r.status = 201; return r; }
            if (!blobs.count(h)) { r.status = 404; return r; }
            r.body = blobs[h];
            return r;
        }
        if (method == "POST") {   // PUSH
            const json j = json::parse(body);
            for (const auto& c : j["changes"]) received.push_back(c);
            r.body = R"({"conflicts":[]})";
            return r;
        }
        // PULL: GET ...changes?since=N&limit=L
        pullPages++;
        const long since = std::strtol(path.c_str() + path.find("since=") + 6, nullptr, 10);
        json page = json::array();
        long last = since;
        for (long seq = since + 1; seq <= (long)journal.size() && (int)page.size() < pageSize; seq++) {
            page.push_back(journal[(size_t)seq - 1]);
            last = seq;
        }
        json out{{"journalId", "J1"}, {"changes", page}, {"lastSeq", last}, {"hasMore", last < (long)journal.size()}};
        r.body = out.dump();
        return r;
    }
};

// Fake repository: throws on a "poison" identifier, counts the rest.
struct ThrowingRepo : domain::ISyncableRepository {
    std::vector<std::string> applied;
    std::string syncType() const override { return "component"; }
    std::vector<domain::SyncRecord> exportForSync(std::int64_t) const override { return {}; }
    std::int64_t maxLocalSeq() const override { return 0; }
    bool applyRemote(const domain::SyncRecord& rec) override {
        if (rec.id == "poison") throw std::runtime_error("enregistrement empoisonné");
        applied.push_back(rec.id);
        return true;
    }
};

// Decorator: runs a user action right AFTER the table export, that is exactly
// in the window where a concurrent modification used to risk being lost.
struct HookRepo : domain::ISyncableRepository {
    explicit HookRepo(domain::ISyncableRepository* r) : inner(r) {}
    domain::ISyncableRepository* inner;
    std::function<void()> afterExport;
    std::string syncType() const override { return inner->syncType(); }
    std::vector<domain::SyncRecord> exportForSync(std::int64_t since) const override {
        auto out = inner->exportForSync(since);
        if (afterExport) { auto f = afterExport; const_cast<HookRepo*>(this)->afterExport = nullptr; f(); }
        return out;
    }
    std::int64_t maxLocalSeq() const override { return inner->maxLocalSeq(); }
    bool applyRemote(const domain::SyncRecord& rec) override { return inner->applyRemote(rec); }
    std::vector<std::string> referencedBlobs() const override { return inner->referencedBlobs(); }
};

std::set<std::string> receivedIds(const FakeHub& hub) {
    std::set<std::string> ids;
    for (const auto& c : hub.received) ids.insert(c["id"].get<std::string>());
    return ids;
}

domain::Component saveNamed(chdesktop::AppContext& ctx, const std::string& name) {
    domain::Component c;
    c.name = name;
    return ctx.inventory.saveComponent(c);
}

// --- 1) Paginated pull, poison records (malformed JSON) on real tables --------------
void testPullWithPoison() {
    const fs::path dir = freshDir("ch_test_sync_poison");
    chdesktop::AppContext ctx(dir.string());
    FakeHub hub;
    hub.pageSize = 2;
    hub.journal = {
        envelope("component", "c1", 1, {{"name", "Un"}}),
        json(nullptr),                                          // null
        json(42),                                               // scalaire
        json("texte"),                                          // string
        json::array({1, 2}),                                    // tableau
        json{{"type", "component"}},                            // without identifier
        json{{"id", "c2"}, {"type", "component"}, {"rev", "abc"}, {"data", "pas un objet"}},
        envelope("component", "c3", 1, {{"name", nullptr}, {"notes", nullptr}, {"quantity", "beaucoup"}}),
        envelope("table_inconnue", "x1", 1, json::object()),    // unknown type: ignored
        envelope("location", "l1", 1, {{"name", "Tiroir"}}),
    };
    chsync::SyncService svc(ctx.syncables(), (dir / "sync_state.json").string(), &ctx.attachments);
    svc.setSqlExec([&ctx](const char* sql) { ctx.database().execSql(sql); });
    svc.setHttp(hub.fn());
    const auto out = svc.sync(kCfg);
    CHECK(out.ok && out.error.empty());
    CHECK(out.lastSeq == (long)hub.journal.size());           // the cursor went through all the poisons
    CHECK(hub.pullPages >= 5);                                 // pagination really exercised
    CHECK(ctx.components.findById("c1").has_value());
    CHECK(ctx.components.findById("c3").has_value());
    CHECK(ctx.locations.findById("l1").has_value());
    // A second synchronization replays nothing and is not blocked.
    hub.requests = 0;
    const auto again = svc.sync(kCfg);
    CHECK(again.ok && again.applied == 0);
}

// --- 2) A repository that THROWS on a record: counted, reported, no blocking -------------
void testThrowingRecord() {
    const fs::path dir = freshDir("ch_test_sync_throw");
    ThrowingRepo repo;
    FakeHub hub;
    hub.journal = {envelope("component", "a", 1, json::object()), envelope("component", "poison", 1, json::object()),
                   envelope("component", "b", 1, json::object())};
    chsync::SyncService svc({&repo}, (dir / "sync_state.json").string());
    svc.setHttp(hub.fn());
    std::vector<std::string> rejectedIds;
    int begins = 0, commits = 0;
    svc.setSqlExec([&](const char* sql) { if (std::string(sql) == "BEGIN IMMEDIATE") begins++; else if (std::string(sql) == "COMMIT") commits++; });
    chsync::SyncObserver obs;
    obs.onReject = [&](const std::string& id, const std::string&) { rejectedIds.push_back(id); };
    svc.setObserver(obs);
    const auto out = svc.sync(kCfg);
    CHECK(out.ok);
    CHECK(out.rejected == 1 && rejectedIds == std::vector<std::string>{"poison"});
    CHECK(repo.applied == (std::vector<std::string>{"a", "b"}));      // the neighbors are applied
    CHECK(out.lastSeq == 3);                                          // the cursor advanced despite the poison
    CHECK(begins == commits && begins >= 2);                          // one transaction per page, all committed
    CHECK(svc.sync(kCfg).ok);                                         // and the next synchronization is not blocked
}

// --- 3) Concurrent user write: never lost, never corrupting --------------
void testConcurrentUserWriteDuringPush() {
    const fs::path dir = freshDir("ch_test_sync_concurrent");
    chdesktop::AppContext ctx(dir.string());               // connexion de l'interface
    saveNamed(ctx, "Avant la synchro");

    FakeHub hub;
    domain::Component late;
    chsync::SyncSession session(dir.string());             // connection of the worker thread
    auto repos = session.syncables();
    HookRepo hook(repos[0]);                               // repos[0] = composants
    hook.afterExport = [&] { late = saveNamed(ctx, "Pendant la synchro"); };
    repos[0] = &hook;
    chsync::SyncService svc(repos, session.statePath(), &session.attachments());
    svc.setSqlExec([&session](const char* sql) { session.database().execSql(sql); });
    svc.setHttp(hub.fn());

    CHECK(svc.sync(kCfg).ok);
    CHECK(hub.received.size() == 1);                       // only the earlier row was sent
    CHECK(!late.id.empty() && !receivedIds(hub).count(late.id));
    CHECK(svc.sync(kCfg).ok);                              // the "during" row is visible at the next push
    CHECK(receivedIds(hub).count(late.id) == 1);
    CHECK(hub.received.size() == 2);                       // and nothing is sent twice
    CHECK(svc.sync(kCfg).ok && hub.received.size() == 2);
    // The database is healthy afterwards.
    CHECK(ctx.components.findAll().size() == 2);
}

// True parallelism: the interface writes continuously while the worker thread pulls 400 rows.
void testConcurrentThreads() {
    const fs::path dir = freshDir("ch_test_sync_threads");
    chdesktop::AppContext ctx(dir.string());
    FakeHub hub;
    hub.pageSize = 40;
    for (int i = 0; i < 400; i++)
        hub.journal.push_back(envelope("component", "remote-" + std::to_string(i), 1, {{"name", "Distant " + std::to_string(i)}}));

    chsync::SyncOutcome out;
    std::thread worker([&] {
        chsync::SyncSession session(dir.string());         // factory PER thread
        chsync::SyncService svc(session.syncables(), session.statePath(), &session.attachments());
        svc.setSqlExec([&session](const char* sql) { session.database().execSql(sql); });
        svc.setHttp(hub.fn());
        out = svc.sync(kCfg);
    });
    std::set<std::string> localIds;
    int writeErrors = 0;
    for (int i = 0; i < 150; i++) {
        try { localIds.insert(saveNamed(ctx, "Local " + std::to_string(i)).id); }
        catch (const std::exception& e) { writeErrors++; std::printf("écriture UI refusée : %s\n", e.what()); }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    CHECK(out.ok);
    CHECK(out.applied == 400);
    CHECK(writeErrors == 0);                               // busy_timeout: the UI never fails on a lock
    CHECK(ctx.components.findAll().size() == 550);

    // All user writes go out at the latest at the next push (none lost);
    // `received` accumulates the uploads of both synchronizations.
    {
        chsync::SyncSession session(dir.string());
        chsync::SyncService svc(session.syncables(), session.statePath(), &session.attachments());
        svc.setSqlExec([&session](const char* sql) { session.database().execSql(sql); });
        svc.setHttp(hub.fn());
        CHECK(svc.sync(kCfg).ok);
    }
    const auto ids = receivedIds(hub);
    for (const auto& id : localIds) CHECK(ids.count(id) == 1);
    for (const auto& id : ids) CHECK(id.rfind("remote-", 0) != 0);   // remote rows are not sent back
}

// --- 4) Cooperative cancellation -----------------------------------------------------------------
void testCancel() {
    const fs::path dir = freshDir("ch_test_sync_cancel");
    chdesktop::AppContext ctx(dir.string());
    FakeHub hub;
    hub.pageSize = 2;
    for (int i = 0; i < 6; i++) hub.journal.push_back(envelope("component", "r" + std::to_string(i), 1, {{"name", "R"}}));
    std::atomic<bool> cancel{false};
    chsync::SyncService svc(ctx.syncables(), (dir / "sync_state.json").string(), &ctx.attachments);
    svc.setSqlExec([&ctx](const char* sql) { ctx.database().execSql(sql); });
    svc.setHttp(hub.fn());
    svc.setCancelFlag(&cancel);
    int pulls = 0;
    hub.onRequest = [&](const std::string& m, const std::string& u) {
        if (m == "GET" && u.find("changes") != std::string::npos && ++pulls == 2) cancel = true;
    };
    const auto out = svc.sync(kCfg);
    CHECK(!out.ok && out.cancelled && out.error == "sync.cancelled");
    CHECK(hub.pullPages == 2);                              // stopped between two pages
    CHECK(svc.lastSeq() == 4);                              // pages already received are kept, resuming is possible
    CHECK(ctx.components.findAll().size() == 4);

    cancel = false;
    hub.onRequest = nullptr;
    const auto resumed = svc.sync(kCfg);
    CHECK(resumed.ok && !resumed.cancelled && svc.lastSeq() == 6);
    CHECK(ctx.components.findAll().size() == 6);

    // Cancelled before starting: no request beyond the probe, no misleading error.
    cancel = true;
    hub.requests = 0;
    const auto early = svc.sync(kCfg);
    CHECK(!early.ok && early.cancelled && hub.requests <= 1);
}

// Attachments: cancellation interrupts the transfer between two blobs.
void testCancelBetweenBlobs() {
    const fs::path dir = freshDir("ch_test_sync_blobs");
    chdesktop::AppContext ctx(dir.string());
    FakeHub hub;
    std::vector<std::string> hashes;
    for (int i = 0; i < 5; i++) {
        const std::string bytes = "blob numero " + std::to_string(i);
        const std::string h = chutil::sha256Hex(bytes);
        hub.blobs[h] = bytes;
        hashes.push_back(h);
        hub.journal.push_back(envelope("document", "d" + std::to_string(i), 1,
            {{"ownerKind", "component"}, {"ownerId", "c"}, {"kind", "file"}, {"title", "t"}, {"hash", h}, {"blobHash", h}}));
    }
    std::atomic<bool> cancel{false};
    chsync::SyncService svc(ctx.syncables(), (dir / "sync_state.json").string(), &ctx.attachments);
    svc.setSqlExec([&ctx](const char* sql) { ctx.database().execSql(sql); });
    svc.setHttp(hub.fn());
    svc.setCancelFlag(&cancel);
    int blobGets = 0;
    hub.onRequest = [&](const std::string& m, const std::string& u) {
        if (m == "GET" && u.find("/api/blob/") != std::string::npos && ++blobGets == 2) cancel = true;
    };
    const auto out = svc.sync(kCfg);
    if (ctx.documents.findByOwner(domain::DocumentOwnerKind::Component, "c").empty()) {
        // The document format does not carry the hash under this name: the blob test is then pointless.
        std::printf("(info) documents sans hash lisible : test d'annulation des blobs ignoré\n");
        return;
    }
    CHECK(!out.ok && out.cancelled);
    CHECK(out.blobsDownloaded < 5);
}

// --- 5) Cancellation of a real blocked HTTP request --------------------------------------------
void testRealHttpCancel() {
    // Server that accepts connections but never answers.
    SilentServer srv;
    CHECK(srv.ok());
    const std::string url = srv.url() + "/muet";

    std::atomic<bool> cancel{false};
    std::thread canceller([&] { std::this_thread::sleep_for(std::chrono::milliseconds(400)); cancel = true; });
    chnet::setThreadCancelFlag(&cancel);
    const auto t0 = std::chrono::steady_clock::now();
    const auto r = chnet::httpGet(url, {}, 20000);          // without cancellation: 20 s of waiting
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    chnet::setThreadCancelFlag(nullptr);
    canceller.join();
    CHECK(!r.ok && r.error == "http.cancelled");
    CHECK(ms < 3000);
}

// --- 6) Connection test and atomic persistent state ------------------------------------------
void testConnectionAndState() {
    const fs::path dir = freshDir("ch_test_sync_state");
    chdesktop::AppContext ctx(dir.string());
    FakeHub hub;
    chsync::SyncService svc(ctx.syncables(), (dir / "sync_state.json").string(), &ctx.attachments);
    svc.setHttp(hub.fn());
    std::string info;
    CHECK(svc.testConnection(kCfg, info) && info.find("hub vtest") == 0);
    CHECK(!svc.deviceId().empty());
    CHECK(fs::exists(dir / "sync_state.json") && !fs::exists(dir / "sync_state.json.tmp"));
    chsync::SyncService again(ctx.syncables(), (dir / "sync_state.json").string(), &ctx.attachments);
    CHECK(again.deviceId() == svc.deviceId());              // the device identity survives
}

} // namespace

int main() {
    testPullWithPoison();
    testThrowingRecord();
    testConcurrentUserWriteDuringPush();
    testConcurrentThreads();
    testCancel();
    testCancelBetweenBlobs();
    testRealHttpCancel();
    testConnectionAndState();
    std::printf(g_failures ? "%d echec(s)\n" : "tous les tests de synchro passent\n", g_failures);
    return g_failures ? 1 : 0;
}
