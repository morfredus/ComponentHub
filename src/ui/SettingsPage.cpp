#include "ui/SettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QCheckBox>
#include <QMessageBox>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Theme.h"
#include "ui/SyncPrefs.h"
#include "ui/Language.h"
#include "sync/SyncService.h"
#include "sync/SyncRunner.h"

SettingsPage::SettingsPage(chdesktop::AppContext& ctx, chui::SyncRunner* runner, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(14);
    v->addWidget(uikit::pageTitle(chui::tr("Settings")));

    auto* language = new QGroupBox(chui::tr("Language"));
    auto* languageLayout = new QHBoxLayout(language);
    languageLayout->addWidget(new QLabel(chui::tr("Interface:")));
    auto* languageBox = new QComboBox;
    for (const auto& l : chui::availableLanguages())
        languageBox->addItem(l.name, l.code);
    languageBox->setCurrentIndex(languageBox->findData(chui::language()));
    languageLayout->addWidget(languageBox);
    languageLayout->addStretch(1);
    v->addWidget(language);
    connect(languageBox, &QComboBox::currentIndexChanged, this, [this, languageBox](int) {
        chui::setLanguage(languageBox->currentData().toString());
        chui::applyLanguage();
        emit localeChanged();   // applied immediately, without restart
    });

    auto* currency = new QGroupBox(chui::tr("Currency"));
    auto* currencyLayout = new QHBoxLayout(currency);
    currencyLayout->addWidget(new QLabel(chui::tr("Currency:")));
    auto* currencyBox = new QComboBox;
    currencyBox->addItem(chui::tr("Euro (€)"), "EUR");
    currencyBox->addItem(chui::tr("US dollar ($)"), "USD");
    currencyBox->addItem(chui::tr("Pound sterling (£)"), "GBP");
    const int currencyIndex = currencyBox->findData(chui::currencyCode(chui::currency()));
    currencyBox->setCurrentIndex(currencyIndex < 0 ? 0 : currencyIndex);
    currencyLayout->addWidget(currencyBox);
    currencyLayout->addStretch(1);
    v->addWidget(currency);
    connect(currencyBox, &QComboBox::currentIndexChanged, this, [this, currencyBox] {
        const QString code = currencyBox->currentData().toString();
        chui::setCurrency(code == "GBP" ? chui::Currency::Pound
                         : code == "USD" ? chui::Currency::Dollar
                                         : chui::Currency::Euro);
        emit localeChanged();   // the displayed amounts must change symbol
    });

    // --- Appearance ---
    auto* appearance = new QGroupBox(chui::tr("Appearance"));
    auto* av = new QVBoxLayout(appearance);
    auto* sys   = new QRadioButton(chui::tr("Follow system"));
    auto* light = new QRadioButton(chui::tr("Light"));
    auto* dark  = new QRadioButton(chui::tr("Dark"));
    av->addWidget(sys); av->addWidget(light); av->addWidget(dark);

    auto* group = new QButtonGroup(this);
    group->addButton(sys,   int(Theme::Mode::System));
    group->addButton(light, int(Theme::Mode::Light));
    group->addButton(dark,  int(Theme::Mode::Dark));
    switch (Theme::instance().mode()) {
        case Theme::Mode::Light: light->setChecked(true); break;
        case Theme::Mode::Dark:  dark->setChecked(true);  break;
        default:                 sys->setChecked(true);   break;
    }
    connect(group, &QButtonGroup::idClicked, this,
            [](int id) { Theme::instance().apply(Theme::Mode(id)); });
    v->addWidget(appearance);

    // --- Data ---
    auto* data = new QGroupBox(chui::tr("Data"));
    auto* dv = new QVBoxLayout(data);
    dv->addWidget(uikit::hint(chui::tr("Folder where the inventory JSON tables are stored.")));
    auto* row = new QHBoxLayout;
    auto* path = new QLineEdit(QString::fromStdString(ctx_.dir()));
    path->setReadOnly(true);
    auto* open = uikit::button(chui::tr("Open folder"), icons::Glyph::Projects, "ghost");
    row->addWidget(path, 1); row->addWidget(open);
    dv->addLayout(row);
    connect(open, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ctx_.dir())));
    });
    v->addWidget(data);

    // --- Synchronization (morfSync) ---
    auto* sync = new QGroupBox(chui::tr("Synchronization (morfSync)"));
    auto* sv = new QVBoxLayout(sync);
    sv->addWidget(uikit::hint(chui::tr("Synchronization hub address on the local network. The inventory works without it; it helps keep the same data on your other computers.")));

    QSettings st("morfredus", "ComponentHub");

    // Explicit choice: use ComponentHub locally only, without any synchronization.
    auto* localOnly = new QCheckBox(chui::tr("Use ComponentHub locally only (disable all synchronization)"));
    localOnly->setChecked(st.value("sync/localOnly", false).toBool());
    sv->addWidget(localOnly);

    auto* urlRow = new QHBoxLayout;
        urlRow->addWidget(new QLabel(chui::tr("Hub address:")));
    auto* urlEdit = new QLineEdit(st.value("sync/serverUrl", "http://morfsync.local:8080").toString());
    urlEdit->setPlaceholderText("http://morfsync.local:8080");
    urlRow->addWidget(urlEdit, 1);
    sv->addLayout(urlRow);

    auto* tokRow = new QHBoxLayout;
    tokRow->addWidget(new QLabel(chui::tr("Token (optional):")));
    auto* tokEdit = new QLineEdit(st.value("sync/token", "").toString());
    tokEdit->setEchoMode(QLineEdit::Password);
    tokEdit->setPlaceholderText(chui::tr("Leave empty if the hub does not require a token."));
    tokRow->addWidget(tokEdit, 1);
    sv->addLayout(tokRow);

    // Automatic options.
    auto* autoStart = new QCheckBox(chui::tr("Synchronize at startup (if the hub is available)"));
    autoStart->setChecked(st.value("sync/autoStart", true).toBool());
    auto* askQuit = new QCheckBox(chui::tr("Offer to synchronize when quitting the application"));
    askQuit->setChecked(st.value("sync/askOnQuit", true).toBool());
    sv->addWidget(autoStart);
    sv->addWidget(askQuit);
    connect(autoStart, &QCheckBox::toggled, this, [](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/autoStart", on);
    });
    connect(askQuit, &QCheckBox::toggled, this, [](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/askOnQuit", on);
    });

    // Saves on every field change.
    auto persist = [urlEdit, tokEdit] {
        QSettings s("morfredus", "ComponentHub");
        s.setValue("sync/serverUrl", urlEdit->text().trimmed());
        s.setValue("sync/token", tokEdit->text());
    };
    connect(urlEdit, &QLineEdit::editingFinished, this, persist);
    connect(tokEdit, &QLineEdit::editingFinished, this, persist);

    auto* status = new QLabel;
    status->setProperty("hint", true);
    status->setWordWrap(true);

    auto* btnRow = new QHBoxLayout;
    auto* testBtn = uikit::button(chui::tr("Test connection"), icons::Glyph::Projects, "ghost");
    auto* syncBtn = uikit::button(chui::tr("Synchronize now"), icons::Glyph::Link);
    auto* cancelBtn = uikit::button(chui::tr("Cancel synchronization"), icons::Glyph::Projects, "ghost");
    cancelBtn->setEnabled(false);
    btnRow->addWidget(testBtn); btnRow->addWidget(syncBtn); btnRow->addWidget(cancelBtn); btnRow->addStretch(1);
    sv->addLayout(btnRow);
    sv->addWidget(status);

    // Metrics of the last synchronization.
    auto* lastSync = new QLabel(chui::lastSyncSummary());
    lastSync->setProperty("hint", true);
    sv->addWidget(lastSync);
    v->addWidget(sync);

    // Button activation: greyed out in local mode AND while a task runs; only
    // "Cancel" is enabled while running.
    auto refreshButtons = [=] {
        const bool local = localOnly->isChecked();
        const bool busy = runner->busy();
        testBtn->setEnabled(!local && !busy);
        syncBtn->setEnabled(!local && !busy);
        cancelBtn->setEnabled(busy);
    };

    // "Local mode only" greys out the whole synchronization section.
    auto applyLocalOnly = [=](bool local) {
        for (QWidget* w : {static_cast<QWidget*>(urlEdit), static_cast<QWidget*>(tokEdit),
                           static_cast<QWidget*>(autoStart), static_cast<QWidget*>(askQuit)})
            w->setEnabled(!local);
        refreshButtons();
        status->setText(local ? chui::tr("Local mode: synchronization is disabled.") : QString());
    };
    applyLocalOnly(localOnly->isChecked());
    connect(localOnly, &QCheckBox::toggled, this, [applyLocalOnly](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/localOnly", on);
        applyLocalOnly(on);
    });

    // Test and synchronization go through the runner's worker thread (never the GUI thread):
    // the buttons are disabled while running, and the result comes back by signal.
    connect(testBtn, &QPushButton::clicked, this, [this, runner, persist, status] {
        persist();
        status->setText(chui::tr("Testing the connection..."));
        if (!runner->start(chui::SyncReason::Test, chui::readSyncConfig()))
            status->setText(chui::tr("A synchronization is already running."));
    });

    connect(syncBtn, &QPushButton::clicked, this, [this, runner, persist, status] {
        persist();
        status->setText(chui::tr("Synchronization in progress..."));
        if (!runner->start(chui::SyncReason::Manual, chui::readSyncConfig()))
            status->setText(chui::tr("A synchronization is already running."));
    });
    connect(cancelBtn, &QPushButton::clicked, runner, &chui::SyncRunner::cancel);

    connect(runner, &chui::SyncRunner::busyChanged, this, [refreshButtons](bool) { refreshButtons(); });
    connect(runner, &chui::SyncRunner::finished, this,
            [status, lastSync](const chui::SyncJobResult& r) {
        if (r.reason == chui::SyncReason::Test) {
            const QString detail = chui::syncError(QString::fromStdString(r.info));
            status->setText(r.ok ? chui::tr("✅ Connected - %1").arg(detail)
                                 : chui::tr("❌ Failed - %1").arg(detail));
        } else if (r.cancelled) {
            status->setText(chui::tr("Synchronization cancelled."));
        } else if (r.hubUnavailable) {
            status->setText(chui::tr("Synchronization hub unavailable - working locally."));
        } else if (!r.ok) {
            status->setText(chui::tr("❌ Failed - %1").arg(chui::syncError(QString::fromStdString(r.outcome.error))));
        } else {
            status->setText(QString(chui::tr("✅ Synchronized - %1 sent, %2 received, %3 conflict(s)."))
                                .arg(r.outcome.accepted).arg(r.outcome.applied).arg(r.outcome.conflicts));
        }
        lastSync->setText(chui::lastSyncSummary());   // already updated by the runner
    });

    // --- About ---
    auto* about = new QLabel(QString(chui::tr("ComponentHub Desktop - version %1\nWorkshop technical memory. © morfredus."))
                                 .arg(CH_APP_VERSION));
    about->setProperty("hint", true);
    v->addWidget(about);

    v->addStretch(1);
}
