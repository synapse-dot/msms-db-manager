#include "Theme.h"
#include <QApplication>
#include <QFontDatabase>

namespace MSMS {

QString Theme::restrainedQss()
{
    // Minimal, restrained styling: clean spacing and alignment without overriding native platform colors.
    return QString::fromUtf8(R"QSS(
/* Restrained platform styling */
QTableView {
    alternate-background-color: palette(alternate-base);
}

QTableView::item {
    padding: 4px 6px;
}

QHeaderView::section {
    padding: 4px 8px;
    font-weight: 600;
}

QTreeView::item {
    padding: 3px 4px;
}

/* Headings and labels */
QLabel[heading="true"] {
    font-size: 16px;
    font-weight: bold;
}

QLabel[subheading="true"] {
    font-size: 12px;
    color: palette(placeholder-text);
}

/* Restrained Panels / Cards */
QFrame[panel="true"] {
    border: 1px solid palette(mid);
    border-radius: 4px;
    background-color: palette(window);
}
)QSS");
}

void Theme::applyNativeTheme(QApplication &app)
{
    // Clear any previously forced dark palettes and use native platform styling
    app.setStyleSheet(restrainedQss());
}

} // namespace MSMS
