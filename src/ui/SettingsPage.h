#pragma once
#include <QWidget>
namespace chdesktop { class AppContext; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
private:
    chdesktop::AppContext& ctx_;
};
