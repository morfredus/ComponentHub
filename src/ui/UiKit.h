// Small interface utilities shared by the pages: titles, buttons with a
// tinted icon that recolor automatically when the theme changes.
#pragma once
#include <QPushButton>
#include <QLabel>
#include <QColor>
#include <QString>
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace uikit {

inline QLabel* pageTitle(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("pageTitle");
    return l;
}

inline QLabel* hint(const QString& text) {
    auto* l = new QLabel(text);
    l->setProperty("hint", true);
    l->setWordWrap(true);
    return l;
}

// Icon button. `variant`: "" (filled/accent), "ghost" (outline), "danger".
inline QPushButton* button(const QString& text, icons::Glyph g, const QString& variant = QString()) {
    auto* b = new QPushButton(text);
    if (!variant.isEmpty()) b->setProperty("variant", variant);
    auto iconColor = [variant]() -> QColor {
        if (variant == "ghost") return QColor(Theme::instance().color("text"));
        return QColor("#ffffff");  // plein/danger : texte blanc
    };
    auto tint = [b, g, iconColor]() { b->setIcon(icons::icon(g, iconColor(), 16)); };
    tint();
    QObject::connect(&Theme::instance(), &Theme::changed, b, tint);
    return b;
}

} // namespace uikit
