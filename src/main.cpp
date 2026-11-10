// ComponentHub Desktop - point d'entrée.
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include "AppContext.h"
#include "platform/Backup.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "ui/Language.h"

#include <morfbeacon/PresenceService.h>
#include <morfbeacon/IMetricsProvider.h>
#include <QJsonObject>

#include <exception>
#include <memory>
#include <functional>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ComponentHub");
    app.setOrganizationName("morfredus");
    app.setApplicationDisplayName("ComponentHub");

    chui::applyLanguage();

    // Dossier de données par utilisateur.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.componenthub";
    QDir().mkpath(dir);

    Theme::instance().apply(Theme::savedMode());

    // Restauration validée lors de la session précédente : à appliquer AVANT d'ouvrir la base
    // (ancienne base conservée en .bak, -wal/-shm supprimés).
    {
        bool applied = false;
        std::string restoreError;
        if (!chdesktop::backup::applyPendingRestore(dir.toStdString(), applied, restoreError))
            QMessageBox::warning(nullptr, chui::tr("ComponentHub - restauration"),
                chui::tr("La restauration en attente n'a pas pu être appliquée ; les données actuelles sont conservées.\n\n")
                    + chui::archiveError(QString::fromStdString(restoreError)));
    }

    std::unique_ptr<chdesktop::AppContext> context;
    try {
        context = std::make_unique<chdesktop::AppContext>(dir.toStdString());
    } catch (const std::exception& e) {
        const QString error = QString::fromUtf8(e.what());
        const bool openFailed = error.startsWith("sqlite_open_failed:");
        const bool initializeFailed = error.startsWith("sqlite_initialize_failed:");
        const QString path = (openFailed || initializeFailed)
            ? error.section(':', 1) : error;
        const QString message = openFailed
            ? chui::tr("The local SQLite database could not be opened.")
            : initializeFailed
                ? chui::tr("The local SQLite database could not be initialized.")
                : chui::tr("The local database could not be opened.");
        QMessageBox::critical(nullptr, chui::tr("ComponentHub - startup failed"),
            message + (path.isEmpty() ? QString() : "\n\n" + chui::tr("Path: ") + path));
        return 1;
    }
    auto& ctx = *context;
    std::unique_ptr<MainWindow> window;
    std::function<void(bool)> openWindow;
    openWindow = [&](bool startup) {
        window = std::make_unique<MainWindow>(ctx, startup);
        window->setWindowIcon(icons::icon(icons::Glyph::Chip, QColor(Theme::instance().color("accent")), 32));
        // Langue ou devise changée : on reconstruit la fenêtre (tous les textes sont
        // posés à la construction) à la même place, à la même taille, sur la même page.
        // Différé d'un tour de boucle : la fenêtre ne doit pas être détruite pendant
        // l'exécution d'un de ses propres signaux.
        QObject::connect(window.get(), &MainWindow::rebuildRequested, &app, [&] {
            QTimer::singleShot(0, [&] {
                auto old = std::move(window);
                old->prepareForRebuild();
                const QByteArray geometry = old->saveGeometry();   // position, taille, état maximisé
                const int page = old->currentPage();
                openWindow(false);
                window->restoreGeometry(geometry);                 // avant show() : pas de saut visible
                window->setCurrentPage(page);
                window->show();
                old.reset();                                       // après show() : jamais zéro fenêtre
            });
        });
    };
    openWindow(true);
    window->show();

    // --- Supervision LAN : annonce de présence + métriques (morfBeacon) -----
    // Heartbeat UDP « je suis actif » broadcast, plus un endpoint HTTP /status
    // interrogé à la demande par le RaspberryDashboard. Les compteurs sont
    // relus au moment de la requête (fréquence faible), sur le thread Qt.
    morfbeacon::PresenceConfig beaconCfg;
    beaconCfg.appName    = "ComponentHub";
    beaconCfg.version    = CH_APP_VERSION;
    beaconCfg.statusPort = 8880;                // /status propre à ComponentHub (appRange, cf. morfTools/ecosystem.json)

    morfbeacon::FunctionMetricsProvider beaconMetrics([&ctx]() {
        QJsonObject m;
        m["components"] = static_cast<int>(ctx.components.findAll().size());
        m["categories"] = static_cast<int>(ctx.categories.findAll().size());
        m["locations"]  = static_cast<int>(ctx.locations.findAll().size());
        m["projects"]   = static_cast<int>(ctx.projects.findAll().size());
        return m;
    });

    morfbeacon::PresenceService presence(beaconCfg, &beaconMetrics);
    presence.start();

    return app.exec();
}
