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
#define CH_APP_VERSION "0.0.0-dev"   // fallback: the real value comes from CMake
#endif

MainWindow::MainWindow(chdesktop::AppContext& ctx, bool startup, QWidget* parent)
    : QMainWindow(parent), ctx_(ctx) {
    setWindowTitle(QStringLiteral("ComponentHub %1").arg(QString::fromLatin1(CH_APP_VERSION)));
    resize(1180, 760);

    // Synchronization outside the GUI thread: created before the pages (the Settings page connects to it).
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
    // The order of the pages follows exactly that of the navigation entries.
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

    // Changes page; each page reloads in its showEvent (the data
    // may have changed from another screen).
    connect(nav_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) stack_->setCurrentIndex(row);
    });

    connect(&Theme::instance(), &Theme::changed, this, &MainWindow::retintNav);

    nav_->setCurrentRow(0);
    statusBar()->showMessage(chui::tr("Data: %1").arg(QString::fromStdString(ctx_.dir())));

    // Silent check for updates, shortly after the UI is shown.
    if (startup) {
        QTimer::singleShot(2000, this, [this]{ checkForUpdates(false); });
        // Startup synchronization (if enabled and the hub is reachable), right after the UI is shown.
        QTimer::singleShot(600, this, [this]{ maybeSyncOnStartup(); });
    }
}

int MainWindow::currentPage() const { return nav_->currentRow(); }
void MainWindow::setCurrentPage(int row) { if (row >= 0 && row < nav_->count()) nav_->setCurrentRow(row); }
void MainWindow::prepareForRebuild() { if (runner_->busy()) runner_->shutdown(5000); }

void MainWindow::maybeSyncOnStartup() {
    // Local mode only, startup synchronization disabled, or hub not
    // configured: do nothing (syncConfigured() already accounts for local mode).
    if (!chui::syncAutoStart() || !chui::syncConfigured()) return;

    // The hub probe (up to 1.5 s) and the synchronization run in the worker thread: the
    // window stays responsive. Hub unreachable -> simple message, the app stays local.
    statusBar()->showMessage(chui::tr("Looking for synchronization hub..."));
    runSync(int(chui::SyncReason::Startup));
}

void MainWindow::runSync(int reason) {
    if (!runner_->start(chui::SyncReason(reason), chui::readSyncConfig()))
        statusBar()->showMessage(chui::tr("A synchronization is already running."), 4000);
}

void MainWindow::onSyncProgress(int phase, int done, int total) {
    using Phase = chsync::SyncProgress::Phase;
    switch (Phase(phase)) {
    case Phase::Connect:
        statusBar()->showMessage(chui::tr("Synchronization in progress...")); break;
    case Phase::Push:
        statusBar()->showMessage(QString(chui::tr("Synchronization: sending %1 change(s)...")).arg(total)); break;
    case Phase::Pull:
        statusBar()->showMessage(chui::tr("Synchronization: receiving changes...")); break;
    case Phase::Blobs:
        statusBar()->showMessage(QString(chui::tr("Synchronization: attachments %1/%2...")).arg(done).arg(total)); break;
    }
}

void MainWindow::onSyncFinished(const chui::SyncJobResult& r) {
    if (r.reason == chui::SyncReason::Test) return;   // handled by the Settings page
    const chsync::SyncOutcome& o = r.outcome;
    const bool startup = (r.reason == chui::SyncReason::Startup);

    if (r.cancelled) {
        statusBar()->showMessage(chui::tr("Synchronization cancelled."), 5000);
    } else if (r.hubUnavailable) {
        statusBar()->showMessage(chui::tr("Synchronization hub unavailable - working locally."), 5000);
    } else if (r.ok) {
        QString msg = startup
            ? QString(chui::tr("Synchronized at startup: %1 received, %2 sent.")).arg(o.applied).arg(o.accepted)
            : QString(chui::tr("Synchronized: %1 received, %2 sent, %3 conflict(s)."))
                  .arg(o.applied).arg(o.accepted).arg(o.conflicts);
        // Mention attachments only when there were some: the common case
        // (nothing to transfer) must not make the message heavier.
        if (!startup && (o.blobsUploaded || o.blobsDownloaded))
            msg += QString(chui::tr(" Attachments: %1 sent, %2 received."))
                       .arg(o.blobsUploaded).arg(o.blobsDownloaded);
        if (o.rejected)
            msg += QString(chui::tr(" %1 unreadable remote record(s) ignored.")).arg(o.rejected);
        statusBar()->showMessage(msg, 6000);
    } else {
        const QString why = chui::syncError(QString::fromStdString(o.error));
        statusBar()->showMessage(startup ? chui::tr("Startup synchronization failed: %1").arg(why)
                                         : chui::tr("Synchronization failed: %1").arg(why), 6000);
    }

    // The pages reload from the database (the worker thread wrote to it through its own connection).
    if (r.ok || o.applied || o.blobsDownloaded) refreshAfterSync();

    // Synchronization offered on close: finished (successful or not), the window can close.
    if (quitAfterSync_) {
        quitAfterSync_ = false;
        close();
    }
}

void MainWindow::refreshAfterSync() {
    // A modal dialog (record being edited) must not see its data change
    // under it: we retry once it is closed.
    if (QApplication::activeModalWidget()) {
        QTimer::singleShot(1000, this, [this] { refreshAfterSync(); });
        return;
    }
    // Each page reloads in its showEvent: hide then show the current page again.
    if (QWidget* page = stack_->currentWidget()) { page->hide(); page->show(); }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Synchronization running: cooperative cancellation then bounded wait. The window never stays
    // blocked on a hub that does not answer, and the thread is never destroyed while running.
    if (runner_->busy()) {
        quitAfterSync_ = false;
        runner_->shutdown(5000);
        QMainWindow::closeEvent(event);
        return;
    }
    // Offer to synchronize on quit (can be disabled in Settings), only once.
    if (!quitAsked_ && chui::syncAskOnQuit() && chui::syncConfigured()) {
        quitAsked_ = true;
        const auto btn = QMessageBox::question(
            this, chui::tr("Synchronize before quitting"),
            chui::tr("Synchronize your data with morfSync before closing?\n(Can be disabled in Settings → Synchronization.)"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (btn == QMessageBox::Yes) {
            // Synchronization runs in the background: keep the window open until it ends
            // (a new close request cancels it, see the beginning of this method).
            quitAfterSync_ = true;
            statusBar()->showMessage(chui::tr("Synchronizing before closing... (close again to cancel)"));
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
    // manual: also shows "up to date" and errors; startup: silent.
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
    auto* sub = new QLabel(chui::tr("Workshop technical memory"));
    sub->setObjectName("brandSub");
    sub->setWordWrap(true);
    v->addWidget(brand);
    v->addWidget(sub);

    entries_ = {
        {chui::tr("Inventory"), int(icons::Glyph::Inventory)},
        {chui::tr("Categories"), int(icons::Glyph::Categories)},
        {chui::tr("Locations"), int(icons::Glyph::Locations)},
        {chui::tr("Projects"), int(icons::Glyph::Projects)},
        {chui::tr("Reference data"), int(icons::Glyph::Referentials)},
        {chui::tr("Import/Export"), int(icons::Glyph::ImportExport)},
        {chui::tr("Settings"), int(icons::Glyph::Settings)},
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

// Classic menu bar - deliberately duplicates the side navigation
// (mouse AND keyboard/menu access, expected from a native desktop application) and
// adds what the sidebar does not cover: quit, theme, help, about.
void MainWindow::buildMenuBar() {
    auto* bar = menuBar();

    // --- File ---
    auto* fileMenu = bar->addMenu(chui::tr("&File"));
    auto* openData = fileMenu->addAction(chui::tr("Open &data folder"));
    connect(openData, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ctx_.dir())));
    });
    fileMenu->addSeparator();
    auto* quit = fileMenu->addAction(chui::tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);         // Ctrl+Q (Cmd+Q on macOS)
    quit->setMenuRole(QAction::QuitRole);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    // --- Go to: jumps to each page (mirror of the sidebar) ---
    auto* goMenu = bar->addMenu(chui::tr("&Go to"));
    for (int i = 0; i < entries_.size(); ++i) {
        auto* act = goMenu->addAction(entries_[i].label);
        if (i < 9) act->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)));
        connect(act, &QAction::triggered, this, [this, i] { nav_->setCurrentRow(i); });
    }

    // --- View: theme (synchronized with Settings and the OS) ---
    auto* viewMenu = bar->addMenu(chui::tr("&View"));
    auto* themeMenu = viewMenu->addMenu(chui::tr("&Theme"));
    themeActions_ = new QActionGroup(this);
    themeActions_->setExclusive(true);
    struct { const char* label; Theme::Mode mode; } themes[] = {
        {CHUI_NOOP("Follow &system"), Theme::Mode::System},
        {CHUI_NOOP("&Light"),             Theme::Mode::Light},
        {CHUI_NOOP("&Dark"),            Theme::Mode::Dark},
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

    // --- Help ---
    auto* helpMenu = bar->addMenu(chui::tr("&Help", "menu"));
    auto* help = helpMenu->addAction(chui::tr("&Help"));
    help->setShortcut(QKeySequence::HelpContents);  // F1
    connect(help, &QAction::triggered, this, &MainWindow::showHelpDialog);

    auto* updates = helpMenu->addAction(chui::tr("Check for &updates..."));
    connect(updates, &QAction::triggered, this, [this]{ checkForUpdates(true); });

    helpMenu->addSeparator();
    auto* about = helpMenu->addAction(chui::tr("About &ComponentHub"));
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &MainWindow::showAboutDialog);
    auto* aboutQt = helpMenu->addAction(chui::tr("About &Qt"));
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
    const QString text = chui::docHtml("about")
        .arg(QString::fromLatin1(CH_APP_VERSION), QString::fromLatin1(QT_VERSION_STR));

    QMessageBox box(this);
    box.setWindowTitle(chui::tr("About ComponentHub"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.setIconPixmap(icons::pixmap(icons::Glyph::Chip,
                                    QColor(Theme::instance().color("accent")), 56));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::showHelpDialog() {
    const QString html = chui::docHtml("help");

    QDialog dlg(this);
    dlg.setWindowTitle(chui::tr("Help - ComponentHub"));
    dlg.resize(580, 560);
    auto* v = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    v->addWidget(browser, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    if (auto* close = buttons->button(QDialogButtonBox::Close)) close->setText(chui::tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    v->addWidget(buttons);
    dlg.exec();
}
