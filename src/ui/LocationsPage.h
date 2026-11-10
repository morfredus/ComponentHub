#pragma once
#include <QWidget>
#include "ids.h"
class QTreeWidget;
class QTreeWidgetItem;
namespace chdesktop { class AppContext; }

class LocationsPage : public QWidget {
    Q_OBJECT
public:
    explicit LocationsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void refresh();
    void addLocation(bool asChild);
    void renameLocation();
    void deleteLocation();
    void moveLocation(QTreeWidgetItem* src, QTreeWidgetItem* newParent);
    domain::Id currentId() const;
    chdesktop::AppContext& ctx_;
    QTreeWidget* tree_ = nullptr;
};
