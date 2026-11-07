// Test du SyncRunner (Qt Core, sans interface) : cycle de vie du thread de travail, refus d'une
// seconde tâche, annulation, arrêt borné à la fermeture. Un hub « muet » (accepte sans répondre)
// garde la tâche bloquée dans le réseau, donc annulable.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include "AppContext.h"
#include "sync/SyncRunner.h"
#include "silent_server.h"
#include "test_util.h"

using chui::SyncReason;

// Attend (en traitant les événements Qt) que `cond` soit vraie, au plus `ms`.
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
    { chdesktop::AppContext ctx(dir.string()); }   // crée le schéma que le worker ouvrira

    SilentServer silent;
    CHECK(silent.ok());
    chsync::SyncConfig silentCfg{silent.url(), "", "componenthub"};
    chsync::SyncConfig deadCfg{"http://127.0.0.1:1", "", "componenthub"};   // refus immédiat

    {   // Test de connexion en échec : résultat reçu dans le thread GUI, plus occupé ensuite.
        chui::SyncRunner runner(dir.string());
        std::vector<bool> busyStates;
        std::vector<chui::SyncJobResult> results;
        QObject::connect(&runner, &chui::SyncRunner::busyChanged, [&](bool b) { busyStates.push_back(b); });
        QObject::connect(&runner, &chui::SyncRunner::finished,
                         [&](const chui::SyncJobResult& r) { results.push_back(r); });
        CHECK(runner.start(SyncReason::Test, deadCfg));
        CHECK(runner.busy());
        CHECK(!runner.start(SyncReason::Manual, deadCfg));             // une seule tâche à la fois
        CHECK(waitFor([&] { return !results.empty(); }, 10000));
        CHECK(results.size() == 1 && results[0].reason == SyncReason::Test && !results[0].ok);
        CHECK(!results[0].info.empty());
        CHECK(!runner.busy());
        CHECK(busyStates == (std::vector<bool>{true, false}));
        // Relançable après la fin (le signal finished est émis APRÈS busyChanged(false)).
        CHECK(runner.start(SyncReason::Manual, deadCfg));
        CHECK(waitFor([&] { return results.size() == 2; }, 10000));
        CHECK(!results[1].ok && !results[1].cancelled);
    }

    {   // Annulation d'une synchro bloquée sur un hub muet.
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

    {   // Fermeture : shutdown() annule et rend la main vite, sans détruire un thread en cours.
        auto runner = std::make_unique<chui::SyncRunner>(dir.string());
        CHECK(runner->start(SyncReason::Manual, silentCfg));
        QCoreApplication::processEvents();
        QElapsedTimer t;
        t.start();
        CHECK(runner->shutdown(5000));
        CHECK(t.elapsed() < 4000);
        runner.reset();                                                // destruction sans plantage
    }

    std::printf(g_failures ? "%d echec(s)\n" : "tests du runner : ok\n", g_failures);
    return g_failures ? 1 : 0;
}
