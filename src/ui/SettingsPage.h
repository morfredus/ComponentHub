#pragma once
#include <QWidget>
namespace chdesktop { class AppContext; }
namespace chui { class SyncRunner; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(chdesktop::AppContext& ctx, chui::SyncRunner* runner, QWidget* parent = nullptr);
private:
    chdesktop::AppContext& ctx_;
};
