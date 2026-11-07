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
    v->addWidget(uikit::pageTitle(chui::tr("Réglages")));

    auto* language = new QGroupBox(chui::tr("Langue"));
    auto* languageLayout = new QHBoxLayout(language);
    languageLayout->addWidget(new QLabel(chui::tr("Interface:")));
    auto* languageBox = new QComboBox;
    languageBox->addItem("Français", "fr");
    languageBox->addItem("English", "en");
    languageBox->setCurrentIndex(chui::language() == chui::Language::English ? 1 : 0);
    languageLayout->addWidget(languageBox);
    languageLayout->addStretch(1);
    v->addWidget(language);
    connect(languageBox, &QComboBox::currentIndexChanged, this, [languageBox](int) {
        chui::setLanguage(languageBox->currentData().toString() == "en"
                              ? chui::Language::English : chui::Language::French);
        QMessageBox::information(nullptr, "Language / Langue",
            chui::tr("Redémarrez ComponentHub pour appliquer la langue."));
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
    connect(currencyBox, &QComboBox::currentIndexChanged, this, [currencyBox] {
        const QString code = currencyBox->currentData().toString();
        chui::setCurrency(code == "GBP" ? chui::Currency::Pound
                         : code == "USD" ? chui::Currency::Dollar
                                         : chui::Currency::Euro);
    });

    // --- Apparence ---
    auto* appearance = new QGroupBox(chui::tr("Apparence"));
    auto* av = new QVBoxLayout(appearance);
    auto* sys   = new QRadioButton(chui::tr("Suivre le système"));
    auto* light = new QRadioButton(chui::tr("Clair"));
    auto* dark  = new QRadioButton(chui::tr("Sombre"));
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

    // --- Données ---
    auto* data = new QGroupBox(chui::tr("Données"));
    auto* dv = new QVBoxLayout(data);
    dv->addWidget(uikit::hint(chui::tr("Dossier où sont stockées les tables JSON de l'inventaire.")));
    auto* row = new QHBoxLayout;
    auto* path = new QLineEdit(QString::fromStdString(ctx_.dir()));
    path->setReadOnly(true);
    auto* open = uikit::button(chui::tr("Ouvrir le dossier"), icons::Glyph::Projects, "ghost");
    row->addWidget(path, 1); row->addWidget(open);
    dv->addLayout(row);
    connect(open, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ctx_.dir())));
    });
    v->addWidget(data);

    // --- Synchronisation (morfSync) ---
    auto* sync = new QGroupBox(chui::tr("Synchronisation (morfSync)"));
    auto* sv = new QVBoxLayout(sync);
    sv->addWidget(uikit::hint(chui::tr("Synchronization hub address on the local network. "
                                      "The inventory works without it; it helps keep the same "
                                      "data on your other computers.")));

    QSettings st("morfredus", "ComponentHub");

    // Choix explicite : n'utiliser ComponentHub qu'en local, sans aucune synchro.
    auto* localOnly = new QCheckBox(chui::tr("Utiliser ComponentHub uniquement en local (désactiver toute synchronisation)"));
    localOnly->setChecked(st.value("sync/localOnly", false).toBool());
    sv->addWidget(localOnly);

    auto* urlRow = new QHBoxLayout;
        urlRow->addWidget(new QLabel(chui::tr("Adresse du hub :")));
    auto* urlEdit = new QLineEdit(st.value("sync/serverUrl", "http://morfsync.local:8080").toString());
    urlEdit->setPlaceholderText("http://morfsync.local:8080");
    urlRow->addWidget(urlEdit, 1);
    sv->addLayout(urlRow);

    auto* tokRow = new QHBoxLayout;
    tokRow->addWidget(new QLabel(chui::tr("Jeton (facultatif) :")));
    auto* tokEdit = new QLineEdit(st.value("sync/token", "").toString());
    tokEdit->setEchoMode(QLineEdit::Password);
    tokEdit->setPlaceholderText(chui::tr("Leave empty if the hub does not require a token."));
    tokRow->addWidget(tokEdit, 1);
    sv->addLayout(tokRow);

    // Options automatiques.
    auto* autoStart = new QCheckBox(chui::tr("Synchroniser au démarrage (si le hub est disponible)"));
    autoStart->setChecked(st.value("sync/autoStart", true).toBool());
    auto* askQuit = new QCheckBox(chui::tr("Proposer une synchronisation en quittant l'application"));
    askQuit->setChecked(st.value("sync/askOnQuit", true).toBool());
    sv->addWidget(autoStart);
    sv->addWidget(askQuit);
    connect(autoStart, &QCheckBox::toggled, this, [](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/autoStart", on);
    });
    connect(askQuit, &QCheckBox::toggled, this, [](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/askOnQuit", on);
    });

    // Enregistre à chaque modification de champ.
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
    auto* testBtn = uikit::button(chui::tr("Tester la connexion"), icons::Glyph::Projects, "ghost");
    auto* syncBtn = uikit::button(chui::tr("Synchroniser maintenant"), icons::Glyph::Link);
    auto* cancelBtn = uikit::button(chui::tr("Annuler la synchronisation"), icons::Glyph::Projects, "ghost");
    cancelBtn->setEnabled(false);
    btnRow->addWidget(testBtn); btnRow->addWidget(syncBtn); btnRow->addWidget(cancelBtn); btnRow->addStretch(1);
    sv->addLayout(btnRow);
    sv->addWidget(status);

    // Métriques de la dernière synchronisation.
    auto* lastSync = new QLabel(chui::lastSyncSummary());
    lastSync->setProperty("hint", true);
    sv->addWidget(lastSync);
    v->addWidget(sync);

    // Activation des boutons : grisés en mode local ET pendant qu'une tâche tourne ; seul
    // « Annuler » ne s'active que pendant l'exécution.
    auto refreshButtons = [=] {
        const bool local = localOnly->isChecked();
        const bool busy = runner->busy();
        testBtn->setEnabled(!local && !busy);
        syncBtn->setEnabled(!local && !busy);
        cancelBtn->setEnabled(busy);
    };

    // « Mode local uniquement » grise toute la section synchro.
    auto applyLocalOnly = [=](bool local) {
        for (QWidget* w : {static_cast<QWidget*>(urlEdit), static_cast<QWidget*>(tokEdit),
                           static_cast<QWidget*>(autoStart), static_cast<QWidget*>(askQuit)})
            w->setEnabled(!local);
        refreshButtons();
        status->setText(local ? chui::tr("Mode local : la synchronisation est désactivée.") : QString());
    };
    applyLocalOnly(localOnly->isChecked());
    connect(localOnly, &QCheckBox::toggled, this, [applyLocalOnly](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/localOnly", on);
        applyLocalOnly(on);
    });

    // Test et synchro passent par le thread de travail du runner (jamais par le thread GUI) :
    // les boutons sont désactivés pendant l'exécution, et le résultat revient par signal.
    connect(testBtn, &QPushButton::clicked, this, [this, runner, persist, status] {
        persist();
        status->setText(chui::tr("Test de la connexion…"));
        if (!runner->start(chui::SyncReason::Test, chui::readSyncConfig()))
            status->setText(chui::tr("Une synchronisation est déjà en cours."));
    });

    connect(syncBtn, &QPushButton::clicked, this, [this, runner, persist, status] {
        persist();
        status->setText(chui::tr("Synchronisation en cours…"));
        if (!runner->start(chui::SyncReason::Manual, chui::readSyncConfig()))
            status->setText(chui::tr("Une synchronisation est déjà en cours."));
    });
    connect(cancelBtn, &QPushButton::clicked, runner, &chui::SyncRunner::cancel);

    connect(runner, &chui::SyncRunner::busyChanged, this, [refreshButtons](bool) { refreshButtons(); });
    connect(runner, &chui::SyncRunner::finished, this,
            [status, lastSync](const chui::SyncJobResult& r) {
        if (r.reason == chui::SyncReason::Test) {
            status->setText((r.ok ? chui::tr("✅ Connecté - ") : chui::tr("❌ Échec - "))
                            + chui::syncError(QString::fromStdString(r.info)));
        } else if (r.cancelled) {
            status->setText(chui::tr("Synchronisation annulée."));
        } else if (r.hubUnavailable) {
            status->setText(chui::tr("Hub de synchronisation indisponible - travail en local."));
        } else if (!r.ok) {
            status->setText(chui::tr("❌ Échec - ") + chui::syncError(QString::fromStdString(r.outcome.error)));
        } else {
            status->setText(QString(chui::tr("✅ Synchronisé - %1 envoyé(s), %2 reçu(s), %3 conflit(s)."))
                                .arg(r.outcome.accepted).arg(r.outcome.applied).arg(r.outcome.conflicts));
        }
        lastSync->setText(chui::lastSyncSummary());   // déjà mis à jour par le runner
    });

    // --- À propos ---
    auto* about = new QLabel(QString(chui::tr("ComponentHub Desktop - version %1\n"
                                               "Mémoire technique d'atelier. © morfredus."))
                                 .arg(CH_APP_VERSION));
    about->setProperty("hint", true);
    v->addWidget(about);

    v->addStretch(1);
}
