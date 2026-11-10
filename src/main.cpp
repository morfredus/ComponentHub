// ComponentHub Desktop - entry point.
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

    // Per-user data folder.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.componenthub";
    QDir().mkpath(dir);

    Theme::instance().apply(Theme::savedMode());

    // Restore validated during the previous session: apply it BEFORE opening the database
    // (previous database kept as .bak, -wal/-shm removed).
    {
        bool applied = false;
        std::string restoreError;
        if (!chdesktop::backup::applyPendingRestore(dir.toStdString(), applied, restoreError))
            QMessageBox::warning(nullptr, chui::tr("ComponentHub - restore"),
                chui::tr("The pending restore could not be applied; the current data is kept.\n\n")
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
            message + (path.isEmpty() ? QString() : "\n\n" + chui::tr("Path: %1").arg(path)));
        return 1;
    }
    auto& ctx = *context;
    std::unique_ptr<MainWindow> window;
    std::function<void(bool)> openWindow;
    openWindow = [&](bool startup) {
        window = std::make_unique<MainWindow>(ctx, startup);
        window->setWindowIcon(icons::icon(icons::Glyph::Chip, QColor(Theme::instance().color("accent")), 32));
        // Language or currency changed: rebuild the window (all texts are
        // set at construction) at the same place, with the same size, on the same page.
        // Deferred by one event loop turn: the window must not be destroyed while
        // one of its own signals is running.
        QObject::connect(window.get(), &MainWindow::rebuildRequested, &app, [&] {
            QTimer::singleShot(0, [&] {
                auto old = std::move(window);
                old->prepareForRebuild();
                const QByteArray geometry = old->saveGeometry();   // position, size, maximized state
                const int page = old->currentPage();
                openWindow(false);
                window->restoreGeometry(geometry);                 // before show(): no visible jump
                window->setCurrentPage(page);
                window->show();
                old.reset();                                       // after show(): never zero windows
            });
        });
    };
    openWindow(true);
    window->show();

    // --- LAN supervision: presence announcement + metrics (morfBeacon) -----
    // UDP "I am alive" heartbeat broadcast, plus an HTTP /status endpoint
    // queried on demand by the RaspberryDashboard. The counters are
    // re-read when the request arrives (low frequency), on the Qt thread.
    morfbeacon::PresenceConfig beaconCfg;
    beaconCfg.appName    = "ComponentHub";
    beaconCfg.version    = CH_APP_VERSION;
    beaconCfg.statusPort = 8880;                // ComponentHub's own /status (appRange, see morfTools/ecosystem.json)

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
