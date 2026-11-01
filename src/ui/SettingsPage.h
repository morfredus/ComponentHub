#pragma once
#include <QWidget>
namespace chdesktop { class AppContext; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
signals:
    // Langue ou devise modifiée : l'interface doit être reconstruite pour s'appliquer.
    void localeChanged();
private:
    chdesktop::AppContext& ctx_;
};
