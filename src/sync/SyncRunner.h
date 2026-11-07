/**
 * SyncRunner - exécute la synchronisation (et le test de connexion) dans un thread de travail
 * dédié, hors du thread de l'interface.
 *
 * Règles de conception :
 *  - le thread de travail ouvre SA connexion SQLite et ses propres dépôts (SyncSession) : aucun
 *    objet partagé avec l'interface ;
 *  - rien de l'interface n'est touché depuis ce thread : il ne fait qu'émettre des signaux,
 *    reçus dans le thread GUI par des connexions en file ;
 *  - une seule tâche à la fois (start() refuse si busy()) ;
 *  - annulation coopérative (indicateur atomique) ; à la fermeture, shutdown() annule puis
 *    attend un temps borné, et ne détruit JAMAIS un QThread encore en cours d'exécution.
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

// Origine de la tâche : pilote les messages affichés par l'interface.
enum class SyncReason { Startup, Manual, Quit, Test };

struct SyncJobResult {
    SyncReason reason = SyncReason::Manual;
    bool ok = false;
    bool cancelled = false;
    bool hubUnavailable = false;   // sonde de démarrage restée sans réponse (pas une erreur)
    std::string info;              // test de connexion : résumé du hub ou code d'erreur
    chsync::SyncOutcome outcome;   // synchro : compteurs et code d'erreur
};

// Objet qui VIT dans le thread de travail. Son unique slot, run(), exécute la tâche.
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

// Côté interface : gère le cycle de vie du thread et réémet les signaux du worker.
class SyncRunner : public QObject {
    Q_OBJECT
public:
    explicit SyncRunner(std::string dataDir, QObject* parent = nullptr);
    ~SyncRunner() override;

    bool busy() const { return thread_ != nullptr; }
    // Lance une tâche. Faux (et rien ne démarre) si une autre est déjà en cours.
    bool start(SyncReason reason, const chsync::SyncConfig& cfg);
    // Demande l'arrêt de la tâche en cours (sans attendre).
    void cancel();
    // Annule puis attend au plus `waitMs`. Vrai si plus rien ne tourne. Sur échec le thread est
    // laissé finir seul (jamais détruit en cours d'exécution).
    bool shutdown(int waitMs);

signals:
    void busyChanged(bool busy);
    void progress(int phase, int done, int total);
    void recordRejected(const QString& id, const QString& reason);
    // Émis dans le thread GUI, APRÈS busyChanged(false) : le récepteur peut relancer une tâche.
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
