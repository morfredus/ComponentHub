#pragma once
#include <QWidget>
#include <QString>
#include <optional>
#include "ids.h"
#include "domain/component.h"
class QLineEdit;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QPoint;
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
    void reloadFilters();
    void showContextMenu(const QPoint& pos);
    void duplicateCurrent();
    void moveCurrent();
    void setQuantityCurrent();
    void openDatasheet();
    void adjustStock(int delta, int setTo = -1);
    void selectById(const domain::Id& id);
    domain::Id currentId() const;
    std::optional<domain::Component> findComponent(const domain::Id& id) const;
    void addComponent();
    void editComponentRow(int row);
    void editCurrent();
    void deleteCurrent();
    QString buildPhotoTooltip(int row);

    chdesktop::AppContext& ctx_;
    QLineEdit*    search_ = nullptr;
    QComboBox*    kind_ = nullptr;
    QCheckBox*    lowStock_ = nullptr;
    QComboBox*    location_ = nullptr;
    QComboBox*    category_ = nullptr;
    QComboBox*    status_ = nullptr;
    QTableWidget* table_ = nullptr;
};
