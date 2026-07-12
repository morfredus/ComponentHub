// Theme — apparence centralisée (repris de la mécanique éprouvée de SiteWatch).
//
// Charge app.qss, y injecte les couleurs du thème choisi (light/dark.theme) et
// l'applique. En mode « Système », suit le thème clair/sombre de l'OS (Windows
// et Linux, Qt 6.5+) et réagit à ses changements à chaud.
#pragma once
#include <QObject>
#include <QString>
#include <QHash>

class Theme : public QObject {
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };

    static Theme& instance();

    void apply(Mode mode);           // applique + mémorise (QSettings)
    Mode mode() const { return mode_; }
    static Mode savedMode();         // défaut : Système
    bool isDark() const { return dark_; }
    QString color(const QString& token) const;  // couleur du thème actif

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
