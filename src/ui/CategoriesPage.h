#pragma once
#include <QWidget>
class QListWidget;
namespace chdesktop { class AppContext; }

class CategoriesPage : public QWidget {
    Q_OBJECT
public:
    explicit CategoriesPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void refresh();
    void addCategory();
    void renameCategory();
    void deleteCategory();
    chdesktop::AppContext& ctx_;
    QListWidget* list_ = nullptr;
};
