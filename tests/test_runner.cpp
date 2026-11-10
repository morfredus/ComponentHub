// Test of SyncRunner (Qt Core, no interface): lifecycle of the worker thread, refusal of a
// second task, cancellation, bounded shutdown on close. A "silent" hub (accepts without answering)
// keeps the task blocked in the network, hence cancellable.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include "AppContext.h"
#include "sync/SyncRunner.h"
#include "silent_server.h"
#include "test_util.h"

using chui::SyncReason;

// Waits (processing Qt events) until `cond` is true, at most `ms`.
template <class F>
static bool waitFor(F cond, int ms) {
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return cond();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const fs::path dir = freshDir("ch_test_runner");
    { chdesktop::AppContext ctx(dir.string()); }   // creates the schema that the worker will open

    SilentServer silent;
    CHECK(silent.ok());
    chsync::SyncConfig silentCfg{silent.url(), "", "componenthub"};
    chsync::SyncConfig deadCfg{"http://127.0.0.1:1", "", "componenthub"};   // refus immédiat

    {   // Failing connection test: result received in the GUI thread, not busy afterwards.
        chui::SyncRunner runner(dir.string());
        std::vector<bool> busyStates;
        std::vector<chui::SyncJobResult> results;
        QObject::connect(&runner, &chui::SyncRunner::busyChanged, [&](bool b) { busyStates.push_back(b); });
        QObject::connect(&runner, &chui::SyncRunner::finished,
                         [&](const chui::SyncJobResult& r) { results.push_back(r); });
        CHECK(runner.start(SyncReason::Test, deadCfg));
        CHECK(runner.busy());
        CHECK(!runner.start(SyncReason::Manual, deadCfg));             // one task at a time
        CHECK(waitFor([&] { return !results.empty(); }, 10000));
        CHECK(results.size() == 1 && results[0].reason == SyncReason::Test && !results[0].ok);
        CHECK(!results[0].info.empty());
        CHECK(!runner.busy());
        CHECK(busyStates == (std::vector<bool>{true, false}));
        // Can be restarted after the end (the finished signal is emitted AFTER busyChanged(false)).
        CHECK(runner.start(SyncReason::Manual, deadCfg));
        CHECK(waitFor([&] { return results.size() == 2; }, 10000));
        CHECK(!results[1].ok && !results[1].cancelled);
    }

    {   // Cancellation of a synchronization blocked on a silent hub.
        chui::SyncRunner runner(dir.string());
        std::vector<chui::SyncJobResult> results;
        QObject::connect(&runner, &chui::SyncRunner::finished,
                         [&](const chui::SyncJobResult& r) { results.push_back(r); });
        CHECK(runner.start(SyncReason::Manual, silentCfg));
        QTimer::singleShot(400, &runner, [&] { runner.cancel(); });
        QElapsedTimer t;
        t.start();
        CHECK(waitFor([&] { return !results.empty(); }, 8000));
        CHECK(t.elapsed() < 4000);
        CHECK(!results.empty() && results[0].cancelled && !results[0].ok);
    }

    {   // Close: shutdown() cancels and returns quickly, without destroying a running thread.
        auto runner = std::make_unique<chui::SyncRunner>(dir.string());
        CHECK(runner->start(SyncReason::Manual, silentCfg));
        QCoreApplication::processEvents();
        QElapsedTimer t;
        t.start();
        CHECK(runner->shutdown(5000));
        CHECK(t.elapsed() < 4000);
        runner.reset();                                                // destruction without a crash
    }

    std::printf(g_failures ? "%d echec(s)\n" : "tests du runner : ok\n", g_failures);
    return g_failures ? 1 : 0;
}
