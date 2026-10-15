#include "Theme.h"
#include <QApplication>
#include <QPalette>
#include <QColor>

namespace MSMS {

QString Theme::catppuccinMochaQss()
{
    return QString::fromUtf8(R"QSS(
/* Catppuccin Mocha QSS */
* {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    font-size: 13px;
    color: #cdd6f4;
}

QMainWindow, QDialog {
    background-color: #1e1e2e;
}

QWidget {
    background-color: #1e1e2e;
    color: #cdd6f4;
    selection-background-color: #45475a;
    selection-color: #cdd6f4;
}

/* Menu Bar */
QMenuBar {
    background-color: #181825;
    color: #cdd6f4;
    border-bottom: 1px solid #313244;
    padding: 2px 6px;
}

QMenuBar::item {
    background: transparent;
    padding: 4px 8px;
    border-radius: 4px;
}

QMenuBar::item:selected {
    background-color: #313244;
    color: #89b4fa;
}

QMenuBar::item:pressed {
    background-color: #45475a;
}

/* Menus */
QMenu {
    background-color: #181825;
    color: #cdd6f4;
    border: 1px solid #313244;
    border-radius: 6px;
    padding: 4px;
}

QMenu::item {
    padding: 6px 24px 6px 12px;
    border-radius: 4px;
}

QMenu::item:selected {
    background-color: #313244;
    color: #89b4fa;
}

QMenu::item:disabled {
    color: #6c7086;
}

QMenu::separator {
    height: 1px;
    background: #313244;
    margin: 4px 8px;
}

/* Navigation Tree */
QTreeView, QTreeWidget {
    background-color: #181825;
    color: #cdd6f4;
    border: none;
    border-right: 1px solid #313244;
    padding: 4px;
}

QTreeView::item, QTreeWidget::item {
    padding: 5px 6px;
    border-radius: 4px;
    margin: 1px 0px;
}

QTreeView::item:hover, QTreeWidget::item:hover {
    background-color: #313244;
}

QTreeView::item:selected, QTreeWidget::item:selected {
    background-color: #45475a;
    color: #89b4fa;
    font-weight: bold;
}

QTreeView::branch {
    background: #181825;
}

/* Tables */
QTableView, QTableWidget {
    background-color: #1e1e2e;
    alternate-background-color: #181825;
    color: #cdd6f4;
    gridline-color: #313244;
    border: 1px solid #313244;
    border-radius: 4px;
    selection-background-color: #313244;
    selection-color: #89b4fa;
}

QTableView::item {
    padding: 6px;
    border: none;
}

QTableView::item:selected {
    background-color: #313244;
    color: #89b4fa;
}

QHeaderView::section {
    background-color: #181825;
    color: #bac2de;
    padding: 6px 8px;
    border: none;
    border-right: 1px solid #313244;
    border-bottom: 2px solid #45475a;
    font-weight: 600;
}

QHeaderView::section:hover {
    background-color: #313244;
}

/* Buttons */
QPushButton {
    background-color: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
    border-radius: 5px;
    padding: 6px 14px;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #45475a;
    border-color: #89b4fa;
    color: #89b4fa;
}

QPushButton:pressed {
    background-color: #585b70;
}

QPushButton:disabled {
    background-color: #181825;
    color: #6c7086;
    border-color: #313244;
}

/* Primary Button Accent */
QPushButton[primary="true"] {
    background-color: #89b4fa;
    color: #11111b;
    border: 1px solid #89b4fa;
    font-weight: 600;
}

QPushButton[primary="true"]:hover {
    background-color: #b4befe;
    border-color: #b4befe;
    color: #11111b;
}

QPushButton[primary="true"]:pressed {
    background-color: #74c7ec;
}

/* Line Edit & Inputs */
QLineEdit, QTextEdit, QPlainTextEdit {
    background-color: #181825;
    color: #cdd6f4;
    border: 1px solid #45475a;
    border-radius: 4px;
    padding: 6px 8px;
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 1px solid #89b4fa;
}

/* Status Bar */
QStatusBar {
    background-color: #11111b;
    color: #a6adc8;
    border-top: 1px solid #313244;
}

QStatusBar::item {
    border: none;
}

/* Splitter */
QSplitter::handle {
    background-color: #313244;
}

QSplitter::handle:horizontal {
    width: 2px;
}

QSplitter::handle:vertical {
    height: 2px;
}

/* Scrollbars */
QScrollBar:vertical {
    background: #181825;
    width: 10px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background: #45475a;
    min-height: 20px;
    border-radius: 5px;
    margin: 2px;
}

QScrollBar::handle:vertical:hover {
    background: #585b70;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar:horizontal {
    background: #181825;
    height: 10px;
    margin: 0px;
}

QScrollBar::handle:horizontal {
    background: #45475a;
    min-width: 20px;
    border-radius: 5px;
    margin: 2px;
}

QScrollBar::handle:horizontal:hover {
    background: #585b70;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}

/* Labels */
QLabel {
    background-color: transparent;
    color: #cdd6f4;
}

QLabel[heading="true"] {
    font-size: 16px;
    font-weight: bold;
    color: #cba6f7;
}

QLabel[subheading="true"] {
    font-size: 13px;
    color: #a6adc8;
}

/* Frame / Panels */
QFrame[panel="true"] {
    background-color: #181825;
    border: 1px solid #313244;
    border-radius: 6px;
}
)QSS");
}

void Theme::applyCatppuccinMocha(QApplication &app)
{
    // Apply QSS
    app.setStyleSheet(catppuccinMochaQss());

    // Apply QPalette fallback
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#1e1e2e"));
    pal.setColor(QPalette::WindowText, QColor("#cdd6f4"));
    pal.setColor(QPalette::Base, QColor("#181825"));
    pal.setColor(QPalette::AlternateBase, QColor("#313244"));
    pal.setColor(QPalette::ToolTipBase, QColor("#181825"));
    pal.setColor(QPalette::ToolTipText, QColor("#cdd6f4"));
    pal.setColor(QPalette::Text, QColor("#cdd6f4"));
    pal.setColor(QPalette::Button, QColor("#313244"));
    pal.setColor(QPalette::ButtonText, QColor("#cdd6f4"));
    pal.setColor(QPalette::BrightText, QColor("#f38ba8"));
    pal.setColor(QPalette::Highlight, QColor("#45475a"));
    pal.setColor(QPalette::HighlightedText, QColor("#89b4fa"));
    app.setPalette(pal);
}

} // namespace MSMS
