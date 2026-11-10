// Theme - centralized appearance (taken from the proven mechanism of SiteWatch).
//
// Loads app.qss, injects the colors of the chosen theme (light/dark.theme) into it and
// applies it. In "System" mode, follows the OS light/dark theme (Windows
// and Linux, Qt 6.5+) and reacts to its changes at runtime.
#pragma once
#include <QObject>
#include <QString>
#include <QHash>

class Theme : public QObject {
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };

    static Theme& instance();

    void apply(Mode mode);           // applies + stores (QSettings)
    Mode mode() const { return mode_; }
    static Mode savedMode();         // default: System
    bool isDark() const { return dark_; }
    QString color(const QString& token) const;  // color of the active theme

signals:
    void changed();

private:
    explicit Theme(QObject* parent = nullptr);
    void reapply();
    bool resolveDark() const;
    QHash<QString, QString> loadPalette(bool dark) const;

    Mode mode_ = Mode::System;
    bool dark_ = false;
    QHash<QString, QString> palette_;
};
