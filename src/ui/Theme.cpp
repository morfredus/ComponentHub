#include "ui/Theme.h"

#include <QApplication>
#include <QStyleHints>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QPalette>

// Qt::ColorScheme / colorSchemeChanged : 6.5+ ; setColorScheme : 6.8+.
#define CH_HAS_COLOR_SCHEME     (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#define CH_HAS_SET_COLOR_SCHEME (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))

namespace {
const char* kSettingsKey = "appearance/themeMode";

QSettings themeSettings() {
    return QSettings(QStringLiteral("morfredus"), QStringLiteral("ComponentHub"));
}
Theme::Mode modeFromString(const QString& s) {
    if (s == "light") return Theme::Mode::Light;
    if (s == "dark")  return Theme::Mode::Dark;
    return Theme::Mode::System;
}
QString modeToString(Theme::Mode m) {
    switch (m) {
        case Theme::Mode::Light: return "light";
        case Theme::Mode::Dark:  return "dark";
        default:                 return "system";
    }
}
} // namespace

Theme& Theme::instance() {
    static Theme theme;
    return theme;
}

Theme::Theme(QObject* parent) : QObject(parent) {
#if CH_HAS_COLOR_SCHEME
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { if (mode_ == Mode::System) reapply(); });
#endif
}

Theme::Mode Theme::savedMode() {
    return modeFromString(themeSettings().value(kSettingsKey, "system").toString());
}

void Theme::apply(Mode mode) {
    mode_ = mode;
    themeSettings().setValue(kSettingsKey, modeToString(mode));
#if CH_HAS_SET_COLOR_SCHEME
    switch (mode) {
        case Mode::Light:  qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);   break;
        case Mode::Dark:   qApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);    break;
        case Mode::System: qApp->styleHints()->setColorScheme(Qt::ColorScheme::Unknown); break;
    }
#endif
    reapply();
}

bool Theme::resolveDark() const {
    switch (mode_) {
        case Mode::Light: return false;
        case Mode::Dark:  return true;
        case Mode::System:
#if CH_HAS_COLOR_SCHEME
            return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
            return qApp->palette().color(QPalette::Window).lightness() < 128;
#endif
    }
    return false;
}

QHash<QString, QString> Theme::loadPalette(bool dark) const {
    QHash<QString, QString> palette;
    QFile file(dark ? ":/themes/dark.theme" : ":/themes/light.theme");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return palette;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (!key.isEmpty() && !value.isEmpty()) palette.insert(key, value);
    }
    return palette;
}

void Theme::reapply() {
    dark_ = resolveDark();
    palette_ = loadPalette(dark_);

    QFile qssFile(":/themes/app.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString qss = QString::fromUtf8(qssFile.readAll());
    for (auto it = palette_.constBegin(); it != palette_.constEnd(); ++it)
        qss.replace("@{" + it.key() + "}", it.value());

    qApp->setStyleSheet(qss);
    emit changed();
}

QString Theme::color(const QString& token) const {
    return palette_.value(token, QStringLiteral("#808080"));
}
