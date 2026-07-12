#include "ui/Icons.h"

#include <QPainter>
#include <QPainterPath>

namespace icons {
namespace {

// Chaque glyphe est décrit dans un repère 24×24, tracé au trait (style « ligne »
// façon Feather) avec le stylo courant. Le painter est déjà mis à l'échelle.
void draw(QPainter& p, Glyph g) {
    auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    auto rect = [&](qreal x, qreal y, qreal w, qreal h, qreal r) { p.drawRoundedRect(QRectF(x, y, w, h), r, r); };
    auto poly = [&](std::initializer_list<QPointF> pts) { p.drawPolyline(QPolygonF(pts)); };

    switch (g) {
    case Glyph::Dashboard:
        rect(3, 3, 8, 8, 1.5); rect(13, 3, 8, 5, 1.5);
        rect(13, 10, 8, 11, 1.5); rect(3, 13, 8, 8, 1.5);
        break;
    case Glyph::Inventory: // bac de rangement
        poly({{3, 8}, {12, 4}, {21, 8}, {12, 12}, {3, 8}});
        poly({{3, 8}, {3, 16}, {12, 20}, {21, 16}, {21, 8}});
        line(12, 12, 12, 20);
        break;
    case Glyph::Chip: { // composant / puce
        rect(6, 6, 12, 12, 1.5);
        for (qreal y : {9.0, 12.0, 15.0}) { line(3, y, 6, y); line(18, y, 21, y); }
        for (qreal x : {9.0, 12.0, 15.0}) { line(x, 3, x, 6); line(x, 18, x, 21); }
        break;
    }
    case Glyph::Categories: { // étiquette + œillet
        QPainterPath tag;
        tag.moveTo(11, 3); tag.lineTo(21, 3); tag.lineTo(21, 13);
        tag.lineTo(12, 22); tag.lineTo(2, 12); tag.lineTo(11, 3); tag.closeSubpath();
        p.drawPath(tag);
        p.drawEllipse(QPointF(16.5, 7.5), 1.4, 1.4);
        break;
    }
    case Glyph::Locations: { // épingle de carte
        QPainterPath pin;
        pin.moveTo(12, 21);
        pin.cubicTo(6, 14, 5, 11, 5, 9);
        pin.arcTo(5, 2, 14, 14, 200, 140);
        pin.cubicTo(19, 11, 18, 14, 12, 21);
        p.drawPath(pin);
        p.drawEllipse(QPointF(12, 9), 2.4, 2.4);
        break;
    }
    case Glyph::Projects: // dossier
        poly({{3, 7}, {9, 7}, {11, 9.5}, {21, 9.5}, {21, 19}, {3, 19}, {3, 7}});
        break;
    case Glyph::ImportExport: // flèches haut/bas
        poly({{8, 8}, {8, 20}}); poly({{5, 11}, {8, 8}, {11, 11}});
        poly({{16, 16}, {16, 4}}); poly({{13, 13}, {16, 16}, {19, 13}});
        break;
    case Glyph::Settings: { // engrenage
        p.drawEllipse(QPointF(12, 12), 3.2, 3.2);
        for (int i = 0; i < 8; ++i) {
            const double a = i * 3.14159265 / 4.0;
            const double c = std::cos(a), s = std::sin(a);
            line(12 + c * 6.2, 12 + s * 6.2, 12 + c * 9.2, 12 + s * 9.2);
        }
        break;
    }
    case Glyph::Documents: // fiche avec coin plié
        poly({{6, 3}, {14, 3}, {19, 8}, {19, 21}, {6, 21}, {6, 3}});
        poly({{14, 3}, {14, 8}, {19, 8}});
        break;
    case Glyph::Referentials: // liste normalisée : puces + lignes
        for (qreal y : {6.0, 12.0, 18.0}) { p.drawEllipse(QPointF(5, y), 1.4, 1.4); line(9, y, 20, y); }
        break;
    case Glyph::Merge: // deux branches qui se rejoignent
        poly({{5, 4}, {5, 9}}); poly({{19, 4}, {19, 9}});
        poly({{5, 9}, {12, 15}, {19, 9}}); line(12, 15, 12, 20);
        break;
    case Glyph::MoveUp:   poly({{6, 14}, {12, 8}, {18, 14}}); break;
    case Glyph::MoveDown: poly({{6, 10}, {12, 16}, {18, 10}}); break;
    case Glyph::Add: line(12, 5, 12, 19); line(5, 12, 19, 12); break;
    case Glyph::Edit: // crayon
        poly({{4, 20}, {4, 16}, {16, 4}, {20, 8}, {8, 20}, {4, 20}});
        line(14, 6, 18, 10);
        break;
    case Glyph::Delete: // corbeille
        line(4, 6, 20, 6); poly({{9, 6}, {9, 4}, {15, 4}, {15, 6}});
        poly({{6, 6}, {7, 21}, {17, 21}, {18, 6}});
        line(10, 10, 10, 17); line(14, 10, 14, 17);
        break;
    case Glyph::Search: p.drawEllipse(QPointF(10.5, 10.5), 6.0, 6.0); line(15, 15, 20, 20); break;
    case Glyph::Backup: // télécharger (sauvegarde)
        line(12, 3, 12, 15); poly({{7, 10}, {12, 15}, {17, 10}});
        poly({{4, 17}, {4, 21}, {20, 21}, {20, 17}});
        break;
    case Glyph::Restore: // téléverser (restaurer)
        line(12, 21, 12, 9); poly({{7, 14}, {12, 9}, {17, 14}});
        poly({{4, 7}, {4, 3}, {20, 3}, {20, 7}});
        break;
    case Glyph::StockIn: // entrée de stock
        poly({{4, 20}, {20, 20}}); line(12, 4, 12, 15); poly({{7, 10}, {12, 15}, {17, 10}});
        break;
    case Glyph::StockOut: // sortie de stock
        poly({{4, 20}, {20, 20}}); line(12, 15, 12, 4); poly({{7, 9}, {12, 4}, {17, 9}});
        break;
    case Glyph::Warning:
        poly({{12, 3}, {22, 20}, {2, 20}, {12, 3}});
        line(12, 9, 12, 14); p.drawEllipse(QPointF(12, 17), 0.6, 0.6);
        break;
    case Glyph::Check: poly({{4, 12}, {10, 18}, {20, 6}}); break;
    case Glyph::Close: line(6, 6, 18, 18); line(18, 6, 6, 18); break;
    case Glyph::Link:
        poly({{9, 15}, {15, 9}});
        { QPainterPath a; a.moveTo(11, 7); a.arcTo(7, 3, 8, 8, 90, -200);
          QPainterPath b; b.moveTo(13, 17); b.arcTo(9, 13, 8, 8, -90, -200);
          p.drawPath(a); p.drawPath(b); }
        break;
    }
}

} // namespace

QPixmap pixmap(Glyph g, const QColor& color, int px) {
    const qreal dpr = 2.0;  // rendu 2× pour rester net sur écrans haute densité
    QPixmap pm(int(px * dpr), int(px * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(px / 24.0, px / 24.0);
    QPen pen(color);
    pen.setWidthF(1.9);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    draw(p, g);
    p.end();
    return pm;
}

QIcon icon(Glyph g, const QColor& color, int px) {
    return QIcon(pixmap(g, color, px));
}

} // namespace icons
