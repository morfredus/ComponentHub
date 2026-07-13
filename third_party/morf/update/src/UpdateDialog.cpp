/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfupdate/UpdateDialog.h"
#include "morfupdate/UpdateChecker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QMessageBox>

namespace morfupdate {

UpdateDialog::UpdateDialog(const QString& appName,
                           const QString& currentVersion,
                           const ReleaseInfo& info,
                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Mise a jour de %1").arg(appName));
    resize(560, 480);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(
        tr("<b>Une nouvelle version de %1 est disponible.</b>").arg(appName), this);
    layout->addWidget(title);

    auto* versions = new QLabel(
        tr("Version installee : <b>%1</b>&nbsp;&nbsp;→&nbsp;&nbsp;"
           "Nouvelle version : <b>%2</b>")
            .arg(currentVersion, info.version.toString()),
        this);
    layout->addWidget(versions);

    if (!info.name.isEmpty()) {
        auto* rel = new QLabel(tr("Release : %1").arg(info.name), this);
        layout->addWidget(rel);
    }

    // Changelog en Markdown (rendu par QTextBrowser).
    auto* notes = new QTextBrowser(this);
    notes->setOpenExternalLinks(true);
    if (info.notes.trimmed().isEmpty())
        notes->setPlainText(tr("(Pas de notes de version.)"));
    else
        notes->setMarkdown(info.notes);
    layout->addWidget(notes, 1);

    // Boutons d'action.
    auto* buttons = new QDialogButtonBox(this);

    auto* pageBtn = buttons->addButton(tr("Voir la release"),
                                       QDialogButtonBox::ActionRole);
    connect(pageBtn, &QPushButton::clicked, this, [info]() {
        if (info.htmlUrl.isValid())
            QDesktopServices::openUrl(info.htmlUrl);
    });

    if (!info.assets.isEmpty()) {
        auto* dlBtn = buttons->addButton(tr("Telecharger"),
                                         QDialogButtonBox::AcceptRole);
        const QUrl assetUrl = info.assets.first().url;
        connect(dlBtn, &QPushButton::clicked, this, [this, assetUrl]() {
            if (assetUrl.isValid())
                QDesktopServices::openUrl(assetUrl);
            accept();
        });
    }

    auto* laterBtn = buttons->addButton(tr("Plus tard"),
                                        QDialogButtonBox::RejectRole);
    connect(laterBtn, &QPushButton::clicked, this, &QDialog::reject);

    layout->addWidget(buttons);
}

void checkAndNotify(QWidget* parent,
                    const QString& appName,
                    const morfUpdateConfig& config,
                    bool silentIfUpToDate) {
    // Parente au widget appelant : vit le temps de la verification puis se
    // detruit avec lui. (checkForUpdates est asynchrone.)
    auto* checker = new UpdateChecker(config, parent);

    QObject::connect(checker, &UpdateChecker::updateAvailable, parent,
                     [parent, appName, config](const ReleaseInfo& info) {
                         auto* dlg = new UpdateDialog(appName, config.currentVersion,
                                                      info, parent);
                         dlg->setAttribute(Qt::WA_DeleteOnClose);
                         dlg->open();
                     });

    if (!silentIfUpToDate) {
        QObject::connect(checker, &UpdateChecker::upToDate, parent,
                         [parent, appName, config](const ReleaseInfo&) {
                             QMessageBox::information(
                                 parent, appName,
                                 QObject::tr("%1 est a jour (version %2).")
                                     .arg(appName, config.currentVersion));
                         });
        QObject::connect(checker, &UpdateChecker::checkFailed, parent,
                         [parent, appName](const QString& err) {
                             QMessageBox::warning(
                                 parent, appName,
                                 QObject::tr("Verification impossible : %1").arg(err));
                         });
    }

    checker->checkForUpdates();
}

} // namespace morfupdate
