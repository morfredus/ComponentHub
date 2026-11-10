#pragma once
#include <QWidget>
namespace chdesktop { class AppContext; }
namespace chui { class SyncRunner; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(chdesktop::AppContext& ctx, chui::SyncRunner* runner, QWidget* parent = nullptr);
signals:
    // Language or currency changed: the window must be rebuilt to translate everything again.
    void localeChanged();
private:
    chdesktop::AppContext& ctx_;
};
