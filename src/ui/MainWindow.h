// MainWindow - coquille de l'application : barre latérale de navigation +
// pile de pages (QStackedWidget). Chaque page reçoit une référence à
// l'AppContext (accès aux services du domaine).
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
    // startup=false : fenêtre reconstruite après un changement de langue/devise
    // (ni vérification de mise à jour ni synchro de démarrage à refaire).
    explicit MainWindow(chdesktop::AppContext& ctx, bool startup = true, QWidget* parent = nullptr);
    int currentPage() const;
    void setCurrentPage(int row);
    void prepareForRebuild();   // arrête proprement une synchro en cours
signals:
    void rebuildRequested();    // langue ou devise changée

protected:
    void closeEvent(QCloseEvent* event) override;  // propose la synchro en quittant

private:
    struct NavEntry { QString label; int glyph; };
    void maybeSyncOnStartup();                 // synchro au démarrage si hub joignable
    void runSync(int reason);                  // lance un cycle dans le thread de travail (chui::SyncReason)
    void onSyncProgress(int phase, int done, int total);
    void onSyncFinished(const chui::SyncJobResult& result);
    void refreshAfterSync();                   // recharge la page affichée depuis la base
    void buildSidebar();
    void buildMenuBar();
    void retintNav();
    void syncThemeMenu();
    void showAboutDialog();
    void showHelpDialog();
    void checkForUpdates(bool manual);   // manual=true : entrée de menu ; false : au démarrage (silencieux)

    chdesktop::AppContext& ctx_;
    chui::SyncRunner* runner_ = nullptr;   // synchro hors du thread GUI (enfant de la fenêtre)
    bool quitAsked_ = false;               // la proposition « synchroniser avant de quitter » a eu lieu
    bool quitAfterSync_ = false;           // fermer la fenêtre dès que la synchro de sortie est finie
    QListWidget*    nav_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QVector<NavEntry> entries_;
    QActionGroup* themeActions_ = nullptr;
};
