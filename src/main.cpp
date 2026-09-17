// ComponentHub Desktop — point d'entrée.
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

#include "AppContext.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Icons.h"

#include <morfbeacon/PresenceService.h>
#include <morfbeacon/IMetricsProvider.h>
#include <QJsonObject>

#include <exception>
#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ComponentHub");
    app.setOrganizationName("morfredus");

    // Dossier de données par utilisateur.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.componenthub";
    QDir().mkpath(dir);

    Theme::instance().apply(Theme::savedMode());

    std::unique_ptr<chdesktop::AppContext> context;
    try {
        context = std::make_unique<chdesktop::AppContext>(dir.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "ComponentHub - démarrage impossible",
            "La base de données locale ne peut pas être ouverte.\n\n" + QString::fromUtf8(e.what()));
        return 1;
    }
    auto& ctx = *context;
    MainWindow window(ctx);
    window.setWindowIcon(icons::icon(icons::Glyph::Chip, QColor(Theme::instance().color("accent")), 32));
    window.show();

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
