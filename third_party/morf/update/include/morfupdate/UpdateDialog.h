/*
 * morfUpdate
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QDialog>
#include "morfupdate/ReleaseInfo.h"
#include "morfupdate/morfUpdateConfig.h"

namespace morfupdate {

// -----------------------------------------------------------------------------
// UpdateDialog : boite de dialogue « une nouvelle version est disponible ».
//
// Affiche version courante vs nouvelle + le changelog (Markdown). Les boutons
// ouvrent la page de la release / le binaire dans le navigateur : le
// telechargement et l'installation restent a la main de l'utilisateur (pas
// d'auto-update silencieux, par prudence). Reutilisable tel quel dans un futur
// mecanisme de mise a jour signee.
// -----------------------------------------------------------------------------
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    UpdateDialog(const QString& appName,
                 const QString& currentVersion,
                 const ReleaseInfo& info,
                 QWidget* parent = nullptr);
};

// -----------------------------------------------------------------------------
// checkAndNotify : cable un UpdateChecker et, si une mise a jour existe, affiche
// l'UpdateDialog. Le tout non bloquant.
//
//   - silentIfUpToDate = true  : ne rien afficher si deja a jour / en cas
//                                d'echec (verification automatique au demarrage)
//   - silentIfUpToDate = false : afficher un message meme si a jour / erreur
//                                (verification manuelle « Rechercher les MAJ »)
// -----------------------------------------------------------------------------
void checkAndNotify(QWidget* parent,
                    const QString& appName,
                    const morfUpdateConfig& config,
                    bool silentIfUpToDate = true);

} // namespace morfupdate
