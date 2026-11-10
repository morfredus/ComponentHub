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
    // Exports the CSV produced by `producer` to a chosen file (prefixed with the
    // UTF-8 BOM for Excel/LibreOffice).
    void exportCsv(const std::function<std::string()>& producer, const QString& defaultName);
    // Imports a CSV chosen through `consumer` and shows the result.
    void importCsv(const std::function<domain::ImportResult(const std::string&)>& consumer);
    void backup();
    void restore();
    void backupConfig();    // backs up the configuration alone (JSON)
    void restoreConfig();   // restores the configuration alone (JSON)

    chdesktop::AppContext& ctx_;
};
