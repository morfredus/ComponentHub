#pragma once
#include <QWidget>
class QLineEdit;
class QComboBox;
class QCheckBox;
class QTableWidget;
namespace chdesktop { class AppContext; }

class InventoryPage : public QWidget {
    Q_OBJECT
public:
    explicit InventoryPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void refresh();
    void addComponent();
    void editComponentRow(int row);
    void editCurrent();
    void deleteCurrent();

    chdesktop::AppContext& ctx_;
    QLineEdit*    search_ = nullptr;
    QComboBox*    kind_ = nullptr;
    QCheckBox*    lowStock_ = nullptr;
    QTableWidget* table_ = nullptr;
};
