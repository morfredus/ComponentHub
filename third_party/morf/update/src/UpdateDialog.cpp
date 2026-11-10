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
#include <QCoreApplication>

namespace morfupdate {

namespace {
// Les textes sources sont en anglais ; l'application hote les traduit par un
// catalogue Qt (contexte "morfupdate"). Sans catalogue, l'anglais s'affiche.
QString mt(const char* text) {
    return QCoreApplication::translate("morfupdate", text);
}
} // namespace

UpdateDialog::UpdateDialog(const QString& appName,
                           const QString& currentVersion,
                           const ReleaseInfo& info,
                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(mt("%1 update").arg(appName));
    resize(560, 480);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(
        mt("<b>A new version of %1 is available.</b>").arg(appName), this);
    layout->addWidget(title);

    auto* versions = new QLabel(
        mt("Installed version: <b>%1</b>&nbsp;&nbsp;→&nbsp;&nbsp;"
           "New version: <b>%2</b>")
            .arg(currentVersion, info.version.toString()),
        this);
    layout->addWidget(versions);

    if (!info.name.isEmpty()) {
        auto* rel = new QLabel(mt("Release: %1").arg(info.name), this);
        layout->addWidget(rel);
    }

    // Changelog en Markdown (rendu par QTextBrowser).
    auto* notes = new QTextBrowser(this);
    notes->setOpenExternalLinks(true);
    if (info.notes.trimmed().isEmpty())
        notes->setPlainText(mt("(No release notes.)"));
    else
        notes->setMarkdown(info.notes);
    layout->addWidget(notes, 1);

    // Boutons d'action.
    auto* buttons = new QDialogButtonBox(this);

    auto* pageBtn = buttons->addButton(mt("View release"),
                                       QDialogButtonBox::ActionRole);
    connect(pageBtn, &QPushButton::clicked, this, [info]() {
        if (info.htmlUrl.isValid())
            QDesktopServices::openUrl(info.htmlUrl);
    });

    // Telecharger le binaire du systeme courant, pas le premier asset venu
    // (souvent checksums.sha256). Si rien ne correspond, on n'offre que la page.
    if (const ReleaseAsset* best = selectAssetForCurrentPlatform(info.assets)) {
        auto* dlBtn = buttons->addButton(mt("Download"),
                                         QDialogButtonBox::AcceptRole);
        const QUrl assetUrl = best->url;
        connect(dlBtn, &QPushButton::clicked, this, [this, assetUrl]() {
            if (assetUrl.isValid())
                QDesktopServices::openUrl(assetUrl);
            accept();
        });
    }

    auto* laterBtn = buttons->addButton(mt("Later"),
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
                                 mt("%1 is up to date (version %2).")
                                     .arg(appName, config.currentVersion));
                         });
        QObject::connect(checker, &UpdateChecker::checkFailed, parent,
                         [parent, appName](const QString& err) {
                             QMessageBox::warning(
                                 parent, appName,
                                 mt("Unable to check for updates: %1").arg(err));
                         });
    }

    checker->checkForUpdates();
}

} // namespace morfupdate
