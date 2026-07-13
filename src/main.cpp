// ComponentHub Desktop — point d'entrée.
#include <QApplication>
#include <QStandardPaths>
#include <QDir>

#include "AppContext.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Icons.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ComponentHub");
    app.setOrganizationName("morfredus");

    // Dossier de données par utilisateur.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.componenthub";
    QDir().mkpath(dir);

    Theme::instance().apply(Theme::savedMode());

    chdesktop::AppContext ctx(dir.toStdString());
    MainWindow window(ctx);
    window.setWindowIcon(icons::icon(icons::Glyph::Chip, QColor(Theme::instance().color("accent")), 32));
    window.show();
    return app.exec();
}
