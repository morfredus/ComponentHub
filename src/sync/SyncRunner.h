/**
 * SyncRunner - runs the synchronization (and the connection test) in a dedicated
 * worker thread, outside the interface thread.
 *
 * Design rules:
 *  - the worker thread opens ITS OWN SQLite connection and its own repositories (SyncSession): no
 *    object is shared with the interface;
 *  - nothing of the interface is touched from this thread: it only emits signals,
 *    received in the GUI thread through queued connections;
 *  - one task at a time (start() refuses if busy());
 *  - cooperative cancellation (atomic flag); on close, shutdown() cancels then
 *    waits for a bounded time, and NEVER destroys a QThread that is still running.
 */
#pragma once
#include <QObject>
#include <QMetaType>
#include <QString>
#include <atomic>
#include <memory>
#include <string>

#include "sync/SyncService.h"

class QThread;

namespace chui {

// Origin of the task: drives the messages shown by the interface.
enum class SyncReason { Startup, Manual, Quit, Test };

struct SyncJobResult {
    SyncReason reason = SyncReason::Manual;
    bool ok = false;
    bool cancelled = false;
    bool hubUnavailable = false;   // startup probe left unanswered (not an error)
    std::string info;              // connection test: hub summary or error code
    chsync::SyncOutcome outcome;   // synchronization: counters and error code
};

// Object that LIVES in the worker thread. Its only slot, run(), executes the task.
class SyncWorker : public QObject {
    Q_OBJECT
public:
    SyncWorker(std::string dataDir, chsync::SyncConfig cfg, SyncReason reason,
               std::shared_ptr<std::atomic<bool>> cancel);
public slots:
    void run();
signals:
    void progress(int phase, int done, int total);   // phase = SyncProgress::Phase
    void rejected(const QString& id, const QString& reason);
    void finished(const chui::SyncJobResult& result);
private:
    std::string dir_;
    chsync::SyncConfig cfg_;
    SyncReason reason_;
    std::shared_ptr<std::atomic<bool>> cancel_;
};

// UI side: manages the thread lifecycle and re-emits the worker's signals.
class SyncRunner : public QObject {
    Q_OBJECT
public:
    explicit SyncRunner(std::string dataDir, QObject* parent = nullptr);
    ~SyncRunner() override;

    bool busy() const { return thread_ != nullptr; }
    // Starts a task. False (and nothing starts) if another one is already running.
    bool start(SyncReason reason, const chsync::SyncConfig& cfg);
    // Requests the current task to stop (without waiting).
    void cancel();
    // Cancels then waits at most `waitMs`. True if nothing runs anymore. On failure the thread is
    // left to finish on its own (never destroyed while running).
    bool shutdown(int waitMs);

signals:
    void busyChanged(bool busy);
    void progress(int phase, int done, int total);
    void recordRejected(const QString& id, const QString& reason);
    // Emitted in the GUI thread, AFTER busyChanged(false): the receiver may start a new task.
    void finished(const chui::SyncJobResult& result);

private slots:
    void onWorkerFinished(const chui::SyncJobResult& result);
    void onThreadFinished();

private:
    std::string dir_;
    QThread* thread_ = nullptr;
    std::shared_ptr<std::atomic<bool>> cancel_;
    SyncJobResult result_;
};

} // namespace chui

Q_DECLARE_METATYPE(chui::SyncJobResult)
