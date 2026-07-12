// MainWindow — coquille de l'application : barre latérale de navigation +
// pile de pages (QStackedWidget). Chaque page reçoit une référence à
// l'AppContext (accès aux services du domaine).
#pragma once
#include <QMainWindow>
#include <QVector>

class QListWidget;
class QStackedWidget;
class QActionGroup;

namespace chdesktop { class AppContext; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(chdesktop::AppContext& ctx, QWidget* parent = nullptr);

private:
    struct NavEntry { QString label; int glyph; };
    void buildSidebar();
    void buildMenuBar();
    void retintNav();
    void syncThemeMenu();
    void showAboutDialog();
    void showHelpDialog();

    chdesktop::AppContext& ctx_;
    QListWidget*    nav_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QVector<NavEntry> entries_;
    QActionGroup* themeActions_ = nullptr;
};
