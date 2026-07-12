#pragma once
#include <QWidget>
#include "ids.h"
class QListWidget;
class QTableWidget;
class QLabel;
namespace chdesktop { class AppContext; }

class ProjectsPage : public QWidget {
    Q_OBJECT
public:
    explicit ProjectsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void refreshProjects();
    void showDetail();
    void addProject();
    void editProject();
    void deleteProject();
    void addBomLine();
    void removeBomLine();
    domain::Id currentProjectId() const;

    chdesktop::AppContext& ctx_;
    QListWidget*  projects_ = nullptr;
    QLabel*       detailHeader_ = nullptr;
    QLabel*       detailMeta_ = nullptr;
    QTableWidget* bom_ = nullptr;
    QLabel*       missing_ = nullptr;
};
