#include "ui/MainWindow.h"

#include <morfupdate/UpdateDialog.h>

#include <QTimer>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QUrl>
#include <QtGlobal>

#include "AppContext.h"
#include "sync/SyncService.h"
#include "sync/SyncRunner.h"
#include "ui/SyncPrefs.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "ui/InventoryPage.h"
#include "ui/CategoriesPage.h"
#include "ui/LocationsPage.h"
#include "ui/ProjectsPage.h"
#include "ui/ReferentialsPage.h"
#include "ui/ImportExportPage.h"
#include "ui/SettingsPage.h"
#include "ui/Language.h"

#ifndef CH_APP_VERSION
#define CH_APP_VERSION "0.0.0-dev"   // repli : la vraie valeur vient de CMake
#endif

MainWindow::MainWindow(chdesktop::AppContext& ctx, bool startup, QWidget* parent)
    : QMainWindow(parent), ctx_(ctx) {
    setWindowTitle(QStringLiteral("ComponentHub %1").arg(QString::fromLatin1(CH_APP_VERSION)));
    resize(1180, 760);

    // Synchro hors du thread GUI : créée avant les pages (la page Réglages s'y connecte).
    runner_ = new chui::SyncRunner(ctx_.dir(), this);
    connect(runner_, &chui::SyncRunner::progress, this, &MainWindow::onSyncProgress);
    connect(runner_, &chui::SyncRunner::finished, this, &MainWindow::onSyncFinished);
    connect(runner_, &chui::SyncRunner::recordRejected, this, [](const QString& id, const QString& why) {
        qWarning("[sync] enregistrement distant rejeté (%s) : %s", qPrintable(id), qPrintable(why));
    });

    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildSidebar();
    buildMenuBar();

    stack_ = new QStackedWidget;
    // L'ordre des pages suit exactement celui des entrées de navigation.
    stack_->addWidget(new InventoryPage(ctx_));
    stack_->addWidget(new CategoriesPage(ctx_));
    stack_->addWidget(new LocationsPage(ctx_));
    stack_->addWidget(new ProjectsPage(ctx_));
    stack_->addWidget(new ReferentialsPage(ctx_));
    stack_->addWidget(new ImportExportPage(ctx_));
    auto* settings = new SettingsPage(ctx_, runner_);
    stack_->addWidget(settings);
    connect(settings, &SettingsPage::localeChanged, this, &MainWindow::rebuildRequested);

    auto* sidebarHost = qobject_cast<QWidget*>(nav_->parentWidget());
    root->addWidget(sidebarHost);
    root->addWidget(stack_, 1);
    setCentralWidget(central);

    // Change de page ; chaque page se recharge dans son showEvent (les données
    // peuvent avoir changé depuis un autre écran).
    connect(nav_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) stack_->setCurrentIndex(row);
    });

    connect(&Theme::instance(), &Theme::changed, this, &MainWindow::retintNav);

    nav_->setCurrentRow(0);
    statusBar()->showMessage(chui::tr("Données : ") + QString::fromStdString(ctx_.dir()));

    // Vérification silencieuse des mises à jour, peu après l'affichage de l'UI.
    if (startup) {
        QTimer::singleShot(2000, this, [this]{ checkForUpdates(false); });
        // Synchro au démarrage (si activée et hub joignable), juste après l'affichage.
        QTimer::singleShot(600, this, [this]{ maybeSyncOnStartup(); });
    }
}

int MainWindow::currentPage() const { return nav_->currentRow(); }
void MainWindow::setCurrentPage(int row) { if (row >= 0 && row < nav_->count()) nav_->setCurrentRow(row); }
void MainWindow::prepareForRebuild() { if (runner_->busy()) runner_->shutdown(5000); }

void MainWindow::maybeSyncOnStartup() {
    // Mode local uniquement, synchro au démarrage désactivée, ou hub non
    // configuré : on ne fait rien (syncConfigured() intègre déjà le mode local).
    if (!chui::syncAutoStart() || !chui::syncConfigured()) return;

    // La sonde du hub (jusqu'à 1,5 s) et la synchro tournent dans le thread de travail : la
    // fenêtre reste réactive. Hub injoignable -> simple message, l'appli reste en local.
    statusBar()->showMessage(chui::tr("Recherche du hub de synchronisation…"));
    runSync(int(chui::SyncReason::Startup));
}

void MainWindow::runSync(int reason) {
    if (!runner_->start(chui::SyncReason(reason), chui::readSyncConfig()))
        statusBar()->showMessage(chui::tr("Une synchronisation est déjà en cours."), 4000);
}

void MainWindow::onSyncProgress(int phase, int done, int total) {
    using Phase = chsync::SyncProgress::Phase;
    switch (Phase(phase)) {
    case Phase::Connect:
        statusBar()->showMessage(chui::tr("Synchronisation en cours…")); break;
    case Phase::Push:
        statusBar()->showMessage(QString(chui::tr("Synchronisation : envoi de %1 modification(s)…")).arg(total)); break;
    case Phase::Pull:
        statusBar()->showMessage(chui::tr("Synchronisation : réception des modifications…")); break;
    case Phase::Blobs:
        statusBar()->showMessage(QString(chui::tr("Synchronisation : pièces jointes %1/%2…")).arg(done).arg(total)); break;
    }
}

void MainWindow::onSyncFinished(const chui::SyncJobResult& r) {
    if (r.reason == chui::SyncReason::Test) return;   // traité par la page Réglages
    const chsync::SyncOutcome& o = r.outcome;
    const bool startup = (r.reason == chui::SyncReason::Startup);

    if (r.cancelled) {
        statusBar()->showMessage(chui::tr("Synchronisation annulée."), 5000);
    } else if (r.hubUnavailable) {
        statusBar()->showMessage(chui::tr("Hub de synchronisation indisponible - travail en local."), 5000);
    } else if (r.ok) {
        QString msg = startup
            ? QString(chui::tr("Synchronisé au démarrage : %1 reçu(s), %2 envoyé(s).")).arg(o.applied).arg(o.accepted)
            : QString(chui::tr("Synchronisé : %1 reçu(s), %2 envoyé(s), %3 conflit(s)."))
                  .arg(o.applied).arg(o.accepted).arg(o.conflicts);
        // Ne mentionner les pièces jointes que s'il y en a eu : le cas courant
        // (rien à transférer) ne doit pas alourdir le message.
        if (!startup && (o.blobsUploaded || o.blobsDownloaded))
            msg += QString(chui::tr(" Pièces jointes : %1 envoyée(s), %2 reçue(s)."))
                       .arg(o.blobsUploaded).arg(o.blobsDownloaded);
        if (o.rejected)
            msg += QString(chui::tr(" %1 enregistrement(s) distant(s) illisible(s) ignoré(s).")).arg(o.rejected);
        statusBar()->showMessage(msg, 6000);
    } else {
        const QString why = chui::syncError(QString::fromStdString(o.error));
        statusBar()->showMessage(startup ? chui::tr("Synchro au démarrage impossible : ") + why
                                         : chui::tr("Synchro impossible : ") + why, 6000);
    }

    // Les pages se rechargent depuis la base (le thread de travail y a écrit via sa connexion).
    if (r.ok || o.applied || o.blobsDownloaded) refreshAfterSync();

    // Synchro proposée à la fermeture : terminée (réussie ou non), la fenêtre peut se fermer.
    if (quitAfterSync_) {
        quitAfterSync_ = false;
        close();
    }
}

void MainWindow::refreshAfterSync() {
    // Une boîte de dialogue modale (fiche en édition) ne doit pas voir ses données changer
    // sous elle : on réessaie une fois qu'elle est fermée.
    if (QApplication::activeModalWidget()) {
        QTimer::singleShot(1000, this, [this] { refreshAfterSync(); });
        return;
    }
    // Chaque page se recharge dans son showEvent : masquer puis ré-afficher la page courante.
    if (QWidget* page = stack_->currentWidget()) { page->hide(); page->show(); }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Synchro en cours : annulation coopérative puis attente bornée. La fenêtre ne reste jamais
    // bloquée sur un hub qui ne répond pas, et le thread n'est jamais détruit en cours.
    if (runner_->busy()) {
        quitAfterSync_ = false;
        runner_->shutdown(5000);
        QMainWindow::closeEvent(event);
        return;
    }
    // Proposition de synchro en quittant (désactivable dans Réglages), une seule fois.
    if (!quitAsked_ && chui::syncAskOnQuit() && chui::syncConfigured()) {
        quitAsked_ = true;
        const auto btn = QMessageBox::question(
            this, chui::tr("Synchroniser avant de quitter"),
            chui::tr("Synchroniser vos données avec morfSync avant de fermer ?\n(Désactivable dans Réglages → Synchronisation.)"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (btn == QMessageBox::Yes) {
            // La synchro tourne en arrière-plan : on garde la fenêtre ouverte jusqu'à sa fin
            // (une nouvelle demande de fermeture l'annule, voir le début de cette méthode).
            quitAfterSync_ = true;
            statusBar()->showMessage(chui::tr("Synchronisation avant la fermeture… (fermer à nouveau pour annuler)"));
            runSync(int(chui::SyncReason::Quit));
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::checkForUpdates(bool manual) {
    morfupdate::morfUpdateConfig cfg;
    cfg.owner          = "morfredus";
    cfg.repo           = "ComponentHub";
    cfg.currentVersion = CH_APP_VERSION;
    // manual : affiche aussi « à jour » et les erreurs ; démarrage : silencieux.
    morfupdate::checkAndNotify(this, "ComponentHub", cfg, /*silentIfUpToDate=*/!manual);
}

void MainWindow::buildSidebar() {
    auto* host = new QWidget;
    host->setObjectName("sidebar");
    host->setFixedWidth(220);
    auto* v = new QVBoxLayout(host);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* brand = new QLabel("ComponentHub");
    brand->setObjectName("brand");
    auto* sub = new QLabel(chui::tr("Mémoire technique d'atelier"));
    sub->setObjectName("brandSub");
    sub->setWordWrap(true);
    v->addWidget(brand);
    v->addWidget(sub);

    entries_ = {
        {chui::tr("Inventaire"), int(icons::Glyph::Inventory)},
        {chui::tr("Catégories"), int(icons::Glyph::Categories)},
        {chui::tr("Emplacements"), int(icons::Glyph::Locations)},
        {chui::tr("Projets"), int(icons::Glyph::Projects)},
        {chui::tr("Référentiels"), int(icons::Glyph::Referentials)},
        {chui::tr("Import/Export"), int(icons::Glyph::ImportExport)},
        {chui::tr("Réglages"), int(icons::Glyph::Settings)},
    };

    nav_ = new QListWidget;
    nav_->setObjectName("nav");
    nav_->setIconSize(QSize(20, 20));
    nav_->setFocusPolicy(Qt::NoFocus);
    for (const auto& e : entries_) {
        auto* it = new QListWidgetItem(e.label);
        nav_->addItem(it);
    }
    v->addWidget(nav_, 1);
    retintNav();
}

void MainWindow::retintNav() {
    const QColor c(Theme::instance().color("sidebarText"));
    for (int i = 0; i < nav_->count() && i < entries_.size(); ++i)
        nav_->item(i)->setIcon(icons::icon(icons::Glyph(entries_[i].glyph), c, 20));
}

// Barre de menus classique - doublonne volontairement la navigation latérale
// (accès souris ET clavier/menu, attendu d'une application de bureau native) et
// ajoute ce que la barre latérale ne couvre pas : quitter, thème, aide, à propos.
void MainWindow::buildMenuBar() {
    auto* bar = menuBar();

    // --- Fichier ---
    auto* fileMenu = bar->addMenu(chui::tr("&Fichier"));
    auto* openData = fileMenu->addAction(chui::tr("Ouvrir le dossier de &données"));
    connect(openData, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ctx_.dir())));
    });
    fileMenu->addSeparator();
    auto* quit = fileMenu->addAction(chui::tr("&Quitter"));
    quit->setShortcut(QKeySequence::Quit);         // Ctrl+Q (Cmd+Q sur macOS)
    quit->setMenuRole(QAction::QuitRole);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    // --- Aller à : saute vers chaque page (miroir de la barre latérale) ---
    auto* goMenu = bar->addMenu(chui::tr("&Aller à"));
    for (int i = 0; i < entries_.size(); ++i) {
        auto* act = goMenu->addAction(entries_[i].label);
        if (i < 9) act->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)));
        connect(act, &QAction::triggered, this, [this, i] { nav_->setCurrentRow(i); });
    }

    // --- Affichage : thème (synchronisé avec les Réglages et l'OS) ---
    auto* viewMenu = bar->addMenu(chui::tr("Afficha&ge"));
    auto* themeMenu = viewMenu->addMenu(chui::tr("&Thème"));
    themeActions_ = new QActionGroup(this);
    themeActions_->setExclusive(true);
    struct { const char* label; Theme::Mode mode; } themes[] = {
        {"Suivre le &système", Theme::Mode::System},
        {"&Clair",             Theme::Mode::Light},
        {"S&ombre",            Theme::Mode::Dark},
    };
    for (const auto& t : themes) {
        auto* act = themeMenu->addAction(chui::tr(t.label));
        act->setCheckable(true);
        act->setData(int(t.mode));
        themeActions_->addAction(act);
        connect(act, &QAction::triggered, this,
                [mode = t.mode] { Theme::instance().apply(mode); });
    }
    syncThemeMenu();
    connect(&Theme::instance(), &Theme::changed, this, &MainWindow::syncThemeMenu);

    // --- Aide ---
    auto* helpMenu = bar->addMenu(chui::tr("Aid&e"));
    auto* help = helpMenu->addAction(chui::tr("&Aide"));
    help->setShortcut(QKeySequence::HelpContents);  // F1
    connect(help, &QAction::triggered, this, &MainWindow::showHelpDialog);

    auto* updates = helpMenu->addAction(chui::tr("Rechercher les mises à &jour…"));
    connect(updates, &QAction::triggered, this, [this]{ checkForUpdates(true); });

    helpMenu->addSeparator();
    auto* about = helpMenu->addAction(chui::tr("À &propos de ComponentHub"));
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &MainWindow::showAboutDialog);
    auto* aboutQt = helpMenu->addAction(chui::tr("À propos de &Qt"));
    aboutQt->setMenuRole(QAction::AboutQtRole);
    connect(aboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::syncThemeMenu() {
    if (!themeActions_) return;
    const int cur = int(Theme::instance().mode());
    for (auto* act : themeActions_->actions())
        act->setChecked(act->data().toInt() == cur);
}

void MainWindow::showAboutDialog() {
    const QString text = chui::tr(
        "<h3 style='margin-bottom:2px'>ComponentHub <span style='font-weight:normal'>%1</span></h3>"
        "<p style='margin-top:0'>Mémoire technique d'atelier - inventaire, stock,"
        " emplacements, projets et documentation.</p>"
        "<p><b>Version</b> : %1<br>"
        "<b>Qt</b> : %2<br>"
        "<b>Licence</b> : <a href='https://www.gnu.org/licenses/gpl-3.0.html'>GPL-3.0-only</a><br>"
        "© morfredus<br>Langues et devises : <a href='https://github.com/nachogarglez'>"
        "Nacho Garcia (@nachogarglez)</a></p>")
        .arg(QString::fromLatin1(CH_APP_VERSION), QString::fromLatin1(QT_VERSION_STR));

    QMessageBox box(this);
    box.setWindowTitle(chui::tr("À propos de ComponentHub"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.setIconPixmap(icons::pixmap(icons::Glyph::Chip,
                                    QColor(Theme::instance().color("accent")), 56));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::showHelpDialog() {
    const QString html = chui::tr(
        "<h2>Bien démarrer avec ComponentHub</h2>"
        "<p>ComponentHub est la <b>mémoire technique de votre atelier</b> : chaque"
        " composant, module, outil ou consommable y a une fiche avec son stock, son"
        " emplacement, sa documentation et les projets qui l'utilisent.</p>"

        "<h3>Les sections (barre de gauche)</h3>"
        "<ul>"
        "<li><b>Inventaire</b> - vos composants, modules, outils et consommables."
        " Recherche globale et filtres ; ouvrez une fiche pour voir/éditer tous ses"
        " champs, ses documents et ses mouvements de stock.</li>"
        "<li><b>Catégories</b> - la liste des catégories de composants.</li>"
        "<li><b>Emplacements</b> - vos rangements, <i>hiérarchisés</i> (Atelier &gt;"
        " Armoire &gt; Tiroir…).</li>"
        "<li><b>Projets</b> - chaque projet et sa nomenclature ; l'application"
        " calcule les composants <i>manquants</i>.</li>"
        "<li><b>Référentiels</b> - les listes normalisées (types, fabricants,"
        " fournisseurs, boîtiers…) pour une saisie homogène.</li>"
        "<li><b>Import/Export</b> - import/export CSV par table et"
        " <b>sauvegarde/restauration complète</b> en une archive <code>.tar</code>.</li>"
        "<li><b>Réglages</b> - thème clair/sombre et dossier de données.</li>"
        "</ul>"

        "<h3>Raccourcis clavier</h3>"
        "<ul>"
        "<li><b>Ctrl+1</b> à <b>Ctrl+7</b> - aller directement à une section.</li>"
        "<li><b>F1</b> - cette aide.</li>"
        "<li><b>Ctrl+Q</b> - quitter.</li>"
        "</ul>"

        "<h3>Où sont mes données ?</h3>"
        "<p>Vos données sont regroupées dans une base SQLite dans votre dossier de données"
        " personnel (menu <i>Fichier → Ouvrir le dossier de données</i>, chemin aussi"
        " rappelé en bas de la fenêtre). Pensez à la <b>sauvegarde <code>.tar</code></b>"
        " (section Import/Export) : elle emporte tout l'atelier en un seul fichier,"
        " à ranger en lieu sûr et à restaurer quand vous voulez.</p>");

    QDialog dlg(this);
    dlg.setWindowTitle(chui::tr("Aide - ComponentHub"));
    dlg.resize(580, 560);
    auto* v = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    v->addWidget(browser, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    if (auto* close = buttons->button(QDialogButtonBox::Close)) close->setText(chui::tr("Fermer"));
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    v->addWidget(buttons);
    dlg.exec();
}
