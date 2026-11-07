#pragma once
#include <QWidget>
namespace chdesktop { class AppContext; }
namespace chui { class SyncRunner; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(chdesktop::AppContext& ctx, chui::SyncRunner* runner, QWidget* parent = nullptr);
signals:
    // Langue ou devise modifiée : l'interface doit être reconstruite pour s'appliquer.
    void localeChanged();
private:
    chdesktop::AppContext& ctx_;
};
