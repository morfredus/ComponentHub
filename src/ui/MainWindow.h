// MainWindow - application shell: navigation sidebar +
// stack of pages (QStackedWidget). Each page receives a reference to
// the AppContext (access to the domain services).
#pragma once
#include <QMainWindow>
#include <QVector>

class QListWidget;
class QStackedWidget;
class QActionGroup;

namespace chdesktop { class AppContext; }
namespace chui { class SyncRunner; struct SyncJobResult; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // startup=false: window rebuilt after a language/currency change
    // (no update check nor startup synchronization to redo).
    explicit MainWindow(chdesktop::AppContext& ctx, bool startup = true, QWidget* parent = nullptr);
    int currentPage() const;
    void setCurrentPage(int row);
    void prepareForRebuild();   // cleanly stops a running synchronization
signals:
    void rebuildRequested();    // language or currency changed

protected:
    void closeEvent(QCloseEvent* event) override;  // offers synchronization on quit

private:
    struct NavEntry { QString label; int glyph; };
    void maybeSyncOnStartup();                 // startup synchronization if the hub is reachable
    void runSync(int reason);                  // starts a cycle in the worker thread (chui::SyncReason)
    void onSyncProgress(int phase, int done, int total);
    void onSyncFinished(const chui::SyncJobResult& result);
    void refreshAfterSync();                   // reloads the displayed page from the database
    void buildSidebar();
    void buildMenuBar();
    void retintNav();
    void syncThemeMenu();
    void showAboutDialog();
    void showHelpDialog();
    void checkForUpdates(bool manual);   // manual=true: menu entry; false: at startup (silent)

    chdesktop::AppContext& ctx_;
    chui::SyncRunner* runner_ = nullptr;   // synchronization outside the GUI thread (child of the window)
    bool quitAsked_ = false;               // the "synchronize before quitting" offer has been made
    bool quitAfterSync_ = false;           // close the window as soon as the exit synchronization is done
    QListWidget*    nav_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QVector<NavEntry> entries_;
    QActionGroup* themeActions_ = nullptr;
};
