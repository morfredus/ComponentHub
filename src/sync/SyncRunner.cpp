#include "sync/SyncRunner.h"

#include <QThread>

#include <exception>

#include "sync/SyncSession.h"
#include "ui/SyncPrefs.h"

namespace chui {

// ---------------------------------------------------------------- SyncWorker (worker thread)

SyncWorker::SyncWorker(std::string dataDir, chsync::SyncConfig cfg, SyncReason reason,
                       std::shared_ptr<std::atomic<bool>> cancel)
    : dir_(std::move(dataDir)), cfg_(std::move(cfg)), reason_(reason), cancel_(std::move(cancel)) {}

void SyncWorker::run() {
    SyncJobResult r;
    r.reason = reason_;
    try {
        // Session owned by THIS thread: SQLite connection, repositories and attachment store.
        chsync::SyncSession session(dir_);
        chsync::SyncService svc(session.syncables(), session.statePath(), &session.attachments());
        svc.setSqlExec([&session](const char* sql) { session.database().execSql(sql); });
        svc.setCancelFlag(cancel_.get());
        chsync::SyncObserver obs;
        obs.onProgress = [this](const chsync::SyncProgress& p) {
            emit progress(static_cast<int>(p.phase), p.done, p.total);
        };
        obs.onReject = [this](const std::string& id, const std::string& why) {
            emit rejected(QString::fromStdString(id), QString::fromStdString(why));
        };
        svc.setObserver(obs);

        if (reason_ == SyncReason::Test) {
            r.ok = svc.testConnection(cfg_, r.info);
        } else {
            // At startup: short probe. If the hub does not answer quickly, the application stays on
            // its local (sovereign) database, without blocking or showing an error.
            bool reachable = true;
            if (reason_ == SyncReason::Startup) {
                std::string info;
                reachable = svc.testConnection(cfg_, info, 1500);
                r.hubUnavailable = !reachable && !cancel_->load();
            }
            if (reachable) {
                r.outcome = svc.sync(cfg_);
                r.ok = r.outcome.ok;
            }
        }
        r.cancelled = cancel_->load() || r.outcome.cancelled;
    } catch (const std::exception& e) {
        // Opening the database failed, etc.: a displayable error, never a crash.
        r.ok = false;
        r.outcome.error = std::string("sync.database_error|") + e.what();
    }
    emit finished(r);
}

// ---------------------------------------------------------------- SyncRunner (GUI thread)

SyncRunner::SyncRunner(std::string dataDir, QObject* parent)
    : QObject(parent), dir_(std::move(dataDir)) {
    qRegisterMetaType<chui::SyncJobResult>("chui::SyncJobResult");
}

SyncRunner::~SyncRunner() { shutdown(5000); }

bool SyncRunner::start(SyncReason reason, const chsync::SyncConfig& cfg) {
    if (thread_) return false;   // one synchronization at a time

    cancel_ = std::make_shared<std::atomic<bool>>(false);
    thread_ = new QThread(this);
    auto* worker = new SyncWorker(dir_, cfg, reason, cancel_);
    worker->moveToThread(thread_);

    // All links to the GUI go through queued connections (emitter in another thread).
    connect(thread_, &QThread::started, worker, &SyncWorker::run);
    connect(worker, &SyncWorker::progress, this, &SyncRunner::progress, Qt::QueuedConnection);
    connect(worker, &SyncWorker::rejected, this, &SyncRunner::recordRejected, Qt::QueuedConnection);
    connect(worker, &SyncWorker::finished, this, &SyncRunner::onWorkerFinished, Qt::QueuedConnection);
    // quit() is thread-safe and MUST be sent directly from the worker thread: queued, it
    // would wait for the GUI loop, which may be blocked in shutdown()/wait().
    connect(worker, &SyncWorker::finished, thread_, &QThread::quit, Qt::DirectConnection);
    connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, &SyncRunner::onThreadFinished, Qt::QueuedConnection);

    thread_->start();
    emit busyChanged(true);
    return true;
}

void SyncRunner::cancel() {
    if (cancel_) cancel_->store(true);
}

bool SyncRunner::shutdown(int waitMs) {
    if (!thread_) return true;
    cancel();
    QThread* t = thread_;
    if (t->wait(waitMs)) return true;   // final cleanup goes through onThreadFinished / ~QObject
    // The task does not stop in time (blocked network request...): it is left to its
    // own fate. The QThread must not die with us while it runs: it is detached from
    // the object tree and deletes itself when it finishes.
    disconnect(t, nullptr, this, nullptr);
    t->setParent(nullptr);
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    thread_ = nullptr;
    return false;
}

void SyncRunner::onWorkerFinished(const SyncJobResult& result) {
    result_ = result;   // releasing the thread (onThreadFinished) will emit the result
}

void SyncRunner::onThreadFinished() {
    QThread* t = thread_;
    thread_ = nullptr;
    if (t) t->deleteLater();   // already finished: safe deletion, in the event loop
    cancel_.reset();

    // Summary of the last successful synchronization, kept here so that every receiver sees it.
    if (result_.ok && result_.reason != SyncReason::Test)
        chui::recordLastSync(result_.outcome.applied, result_.outcome.accepted);

    emit busyChanged(false);
    emit finished(result_);
}

} // namespace chui
