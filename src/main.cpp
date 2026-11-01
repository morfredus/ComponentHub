// ComponentHub Desktop - point d'entrée.
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QTranslator>

#include <functional>

#include "AppContext.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "ui/Language.h"

#include <morfbeacon/PresenceService.h>
#include <morfbeacon/IMetricsProvider.h>
#include <QJsonObject>

#include <exception>
#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ComponentHub");
    app.setOrganizationName("morfredus");
    app.setApplicationDisplayName("ComponentHub");

    // Le traducteur reste installé : on recharge simplement son catalogue quand la
    // langue change (voir showWindow), ce qui évite tout redémarrage.
    QTranslator translator;
    const auto loadTranslation = [&translator] {
        translator.load(QStringLiteral(":/i18n/componenthub_%1.qm").arg(chui::languageCode(chui::language())));
    };
    loadTranslation();
    app.installTranslator(&translator);

    // Dossier de données par utilisateur.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.componenthub";
    QDir().mkpath(dir);

    Theme::instance().apply(Theme::savedMode());

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
    // Les textes sont traduits à la construction des widgets : pour appliquer une
    // nouvelle langue ou devise sans redémarrer, on reconstruit la fenêtre à la
    // même place, sur la même page, puis on détruit l'ancienne.
    std::unique_ptr<MainWindow> window;
    std::function<void(int, bool, const QRect&)> showWindow = [&](int page, bool startup, const QRect& geometry) {
        auto fresh = std::make_unique<MainWindow>(ctx, startup);
        fresh->setWindowIcon(icons::icon(icons::Glyph::Chip, QColor(Theme::instance().color("accent")), 32));
        fresh->goToPage(page);
        if (geometry.isValid()) fresh->setGeometry(geometry);
        QObject::connect(fresh.get(), &MainWindow::rebuildRequested, [&](int p) {
            const QRect geometry = window->geometry();
            loadTranslation();
            std::unique_ptr<MainWindow> old = std::move(window);
            showWindow(p, false, geometry);
            old->hide();
            old.release()->deleteLater();
        });
        window = std::move(fresh);
        window->show();
    };
    showWindow(0, true, QRect());

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
