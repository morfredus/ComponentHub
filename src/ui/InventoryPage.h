#pragma once
#include <QWidget>
#include <QString>
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
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void refresh();
    void addComponent();
    void editComponentRow(int row);
    void editCurrent();
    void deleteCurrent();
    QString buildPhotoTooltip(int row);

    chdesktop::AppContext& ctx_;
    QLineEdit*    search_ = nullptr;
    QComboBox*    kind_ = nullptr;
    QCheckBox*    lowStock_ = nullptr;
    QTableWidget* table_ = nullptr;
};
