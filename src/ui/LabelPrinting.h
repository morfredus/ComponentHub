#pragma once

#include <QMarginsF>
#include <QPageSize>
#include <QPrinter>
#include <QSizeF>

namespace chui {

// Label positions are measured from the physical sheet edge. Apply this both
// before and after the print dialog, which can replace the printer's layout.
inline void configureLabelPrinter(QPrinter& printer, const QSizeF& paperSizeMm) {
    printer.setFullPage(true);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageSize(QPageSize(paperSizeMm, QPageSize::Millimeter));
    printer.setPageMargins(QMarginsF(), QPageLayout::Millimeter);
}

} // namespace chui
