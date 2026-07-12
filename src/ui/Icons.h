// icons — pictogrammes d'interface dessinés vectoriellement (QPainter) sur une
// grille 24×24, teintés à la couleur passée (donc au thème). Aucune police ni
// module SVG requis : rendu net et identique sur Windows, Linux et Raspberry,
// sans dépendance à installer.
#pragma once
#include <QIcon>
#include <QPixmap>
#include <QColor>

namespace icons {

enum class Glyph {
    Dashboard, Inventory, Categories, Locations, Projects, ImportExport, Settings, Documents,
    Referentials, Add, Edit, Delete, Search, Backup, Restore, StockIn, StockOut, Merge,
    MoveUp, MoveDown, Warning, Check, Close, Chip, Link
};

// Pixmap teinté (fond transparent), rendu à `px` pixels, net en haute densité.
QPixmap pixmap(Glyph g, const QColor& color, int px = 20);
QIcon   icon(Glyph g, const QColor& color, int px = 20);

} // namespace icons
