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

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Theme.h"

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

    // --- À propos ---
    auto* about = new QLabel(QString("ComponentHub Desktop — version %1\n"
                                     "Mémoire technique d'atelier. © morfredus.")
                                 .arg(CH_APP_VERSION));
    about->setProperty("hint", true);
    v->addWidget(about);

    v->addStretch(1);
}
