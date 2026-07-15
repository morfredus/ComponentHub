#pragma once
#include <QWidget>
#include <functional>
#include <string>
#include "import_export_service.h"
namespace chdesktop { class AppContext; }

class ImportExportPage : public QWidget {
    Q_OBJECT
public:
    explicit ImportExportPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
private:
    // Exporte le CSV produit par `producer` vers un fichier choisi (préfixé du
    // BOM UTF-8 pour Excel/LibreOffice).
    void exportCsv(const std::function<std::string()>& producer, const QString& defaultName);
    // Importe un CSV choisi via `consumer` et affiche le bilan.
    void importCsv(const std::function<domain::ImportResult(const std::string&)>& consumer);
    void backup();
    void restore();
    void backupConfig();    // sauvegarde la configuration seule (JSON)
    void restoreConfig();   // restaure la configuration seule (JSON)

    chdesktop::AppContext& ctx_;
};
