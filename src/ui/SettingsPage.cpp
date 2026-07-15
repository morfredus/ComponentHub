#include "ui/SettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QCheckBox>
#include <QApplication>
#include <QMessageBox>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Theme.h"
#include "ui/SyncPrefs.h"
#include "sync/SyncService.h"

SettingsPage::SettingsPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(14);
    v->addWidget(uikit::pageTitle("Réglages"));

    // --- Apparence ---
    auto* appearance = new QGroupBox("Apparence");
    auto* av = new QVBoxLayout(appearance);
    auto* sys   = new QRadioButton("Suivre le système");
    auto* light = new QRadioButton("Clair");
    auto* dark  = new QRadioButton("Sombre");
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
    auto* data = new QGroupBox("Données");
    auto* dv = new QVBoxLayout(data);
    dv->addWidget(uikit::hint("Dossier où sont stockées les tables JSON de l'inventaire."));
    auto* row = new QHBoxLayout;
    auto* path = new QLineEdit(QString::fromStdString(ctx_.dir()));
    path->setReadOnly(true);
    auto* open = uikit::button("Ouvrir le dossier", icons::Glyph::Projects, "ghost");
    row->addWidget(path, 1); row->addWidget(open);
    dv->addLayout(row);
    connect(open, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ctx_.dir())));
    });
    v->addWidget(data);

    // --- Synchronisation (HomeServerHub) ---
    auto* sync = new QGroupBox("Synchronisation (HomeServerHub)");
    auto* sv = new QVBoxLayout(sync);
    sv->addWidget(uikit::hint("Adresse du hub de synchronisation sur le réseau local. "
                              "L'inventaire fonctionne sans lui ; il sert à retrouver les mêmes "
                              "données sur vos autres postes."));

    QSettings st("morfredus", "ComponentHub");

    // Choix explicite : n'utiliser ComponentHub qu'en local, sans aucune synchro.
    auto* localOnly = new QCheckBox("Utiliser ComponentHub uniquement en local (désactiver toute synchronisation)");
    localOnly->setChecked(st.value("sync/localOnly", false).toBool());
    sv->addWidget(localOnly);

    auto* urlRow = new QHBoxLayout;
    urlRow->addWidget(new QLabel("Adresse du hub :"));
    auto* urlEdit = new QLineEdit(st.value("sync/serverUrl", "http://homeserverhub.local:8080").toString());
    urlEdit->setPlaceholderText("http://homeserverhub.local:8080");
    urlRow->addWidget(urlEdit, 1);
    sv->addLayout(urlRow);

    auto* tokRow = new QHBoxLayout;
    tokRow->addWidget(new QLabel("Jeton (facultatif) :"));
    auto* tokEdit = new QLineEdit(st.value("sync/token", "").toString());
    tokEdit->setEchoMode(QLineEdit::Password);
    tokEdit->setPlaceholderText("laisser vide si le hub n'exige pas de jeton");
    tokRow->addWidget(tokEdit, 1);
    sv->addLayout(tokRow);

    // Options automatiques.
    auto* autoStart = new QCheckBox("Synchroniser au démarrage (si le hub est disponible)");
    autoStart->setChecked(st.value("sync/autoStart", true).toBool());
    auto* askQuit = new QCheckBox("Proposer une synchronisation en quittant l'application");
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
    auto* testBtn = uikit::button("Tester la connexion", icons::Glyph::Projects, "ghost");
    auto* syncBtn = uikit::button("Synchroniser maintenant", icons::Glyph::Link);
    btnRow->addWidget(testBtn); btnRow->addWidget(syncBtn); btnRow->addStretch(1);
    sv->addLayout(btnRow);
    sv->addWidget(status);

    // Métriques de la dernière synchronisation.
    auto* lastSync = new QLabel(chui::lastSyncSummary());
    lastSync->setProperty("hint", true);
    sv->addWidget(lastSync);
    v->addWidget(sync);

    // « Mode local uniquement » grise toute la section synchro.
    auto applyLocalOnly = [=](bool local) {
        for (QWidget* w : {static_cast<QWidget*>(urlEdit), static_cast<QWidget*>(tokEdit),
                           static_cast<QWidget*>(autoStart), static_cast<QWidget*>(askQuit),
                           static_cast<QWidget*>(testBtn), static_cast<QWidget*>(syncBtn)})
            w->setEnabled(!local);
        status->setText(local ? "Mode local : la synchronisation est désactivée." : QString());
    };
    applyLocalOnly(localOnly->isChecked());
    connect(localOnly, &QCheckBox::toggled, this, [applyLocalOnly](bool on) {
        QSettings("morfredus", "ComponentHub").setValue("sync/localOnly", on);
        applyLocalOnly(on);
    });

    // Un SyncService partagé pour cette page (état persistant dans le dossier de données).
    auto statePath = ctx_.dir() + "/sync_state.json";

    connect(testBtn, &QPushButton::clicked, this, [this, persist, status, statePath] {
        persist();
        chsync::SyncService svc(ctx_.syncables(), statePath);
        std::string info;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool ok = svc.testConnection(chui::readSyncConfig(), info);
        QApplication::restoreOverrideCursor();
        status->setText((ok ? "✅ Connecté — " : "❌ Échec — ") + QString::fromStdString(info));
    });

    connect(syncBtn, &QPushButton::clicked, this, [this, persist, status, lastSync, statePath] {
        persist();
        chsync::SyncService svc(ctx_.syncables(), statePath);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const chsync::SyncOutcome o = svc.sync(chui::readSyncConfig());
        QApplication::restoreOverrideCursor();
        if (!o.ok) {
            status->setText("❌ Échec — " + QString::fromStdString(o.error));
            return;
        }
        status->setText(QString("✅ Synchronisé — %1 envoyé(s), %2 reçu(s), %3 conflit(s).")
                            .arg(o.accepted).arg(o.applied).arg(o.conflicts));
        chui::recordLastSync(o.applied, o.accepted);
        lastSync->setText(chui::lastSyncSummary());
    });

    // --- À propos ---
    auto* about = new QLabel(QString("ComponentHub Desktop — version %1\n"
                                     "Mémoire technique d'atelier. © morfredus.")
                                 .arg(CH_APP_VERSION));
    about->setProperty("hint", true);
    v->addWidget(about);

    v->addStretch(1);
}
