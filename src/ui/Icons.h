// icons - interface pictograms drawn as vectors (QPainter) on a
// 24x24 grid, tinted with the given color (hence with the theme). No font or
// SVG module required: sharp, identical rendering on Windows, Linux and Raspberry,
// with nothing to install.
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

// Tinted pixmap (transparent background), rendered at `px` pixels, sharp on high density.
QPixmap pixmap(Glyph g, const QColor& color, int px = 20);
QIcon   icon(Glyph g, const QColor& color, int px = 20);

} // namespace icons
