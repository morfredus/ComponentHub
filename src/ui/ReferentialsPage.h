// ReferentialsPage - administration of the reference lists (lists of
// standardized values): add, edit, delete, reorder, merge,
// import/export. For lists linked to a component field (type,
// manufacturer, supplier, condition), renames and merges are propagated to the
// components concerned to keep the database consistent.
#pragma once
#include <QWidget>
#include <QString>

class QListWidget;
namespace chdesktop { class AppContext; }

class ReferentialsPage : public QWidget {
    Q_OBJECT
public:
    explicit ReferentialsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void loadDefinitions();
    void selectReferential();
    void reloadValues();
    void addValue();
    void editValue();
    void deleteValue();
    void moveValue(int delta);
    void mergeValues();
    void importCsv();
    void exportCsv();

    QString currentKey() const;
    int currentBind() const;
    void seedIfEmpty(const QString& key, int bind);
    int  reassignComponentField(const QString& from, const QString& to, int bind);  // returns the number of components modified
    int  usageCount(const QString& value, int bind) const;

    chdesktop::AppContext& ctx_;
    QListWidget* referentials_ = nullptr;  // the available lists
    QListWidget* values_ = nullptr;        // values of the selected list
};
