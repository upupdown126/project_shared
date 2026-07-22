#ifndef SMART_HOME_CLIENT_THEME_H
#define SMART_HOME_CLIENT_THEME_H

#include <QString>

namespace client_theme {

inline QString light() {
    return QStringLiteral(R"QSS(
QMainWindow, QWidget { background:#F4F7FB; color:#172033; font-family:"Microsoft YaHei UI"; font-size:13px; }
QWidget#centralPanel { background:#F4F7FB; }
QGroupBox { background:#FFFFFF; border:1px solid #D8E0EC; border-radius:10px; margin-top:12px; padding:14px 10px 10px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; color:#344054; }
QLineEdit, QSpinBox, QPlainTextEdit, QTreeWidget { background:#FFFFFF; color:#172033; border:1px solid #C9D3E1; border-radius:7px; padding:6px 8px; selection-background-color:#2F6FED; selection-color:#FFFFFF; }
QLineEdit:focus, QSpinBox:focus, QPlainTextEdit:focus, QTreeWidget:focus { border:2px solid #4C82F7; padding:5px 7px; }
QTreeWidget { alternate-background-color:#F7F9FC; outline:none; }
QTreeWidget::item { min-height:28px; border-radius:5px; }
QTreeWidget::item:hover { background:#EAF1FF; }
QTreeWidget::item:selected { background:#DCE8FF; color:#194EB5; }
QHeaderView::section { background:#EDF2F8; color:#344054; padding:7px; border:0; border-bottom:1px solid #D8E0EC; font-weight:600; }
QPushButton { background:#FFFFFF; color:#244063; border:1px solid #B9C7DA; border-radius:7px; padding:7px 14px; min-height:18px; font-weight:600; }
QPushButton:hover { background:#EAF1FF; border-color:#6C96EE; color:#1D55BE; }
QPushButton:pressed { background:#BFD3FA; border-color:#245CC5; color:#123D8D; }
QPushButton:checked { background:#2F6FED; border-color:#245CC5; color:#FFFFFF; }
QPushButton:disabled { background:#EDF1F6; color:#9AA7B8; border-color:#D8E0EC; }
QPushButton#themeButton { background:#172033; color:#FFFFFF; border-color:#172033; }
QSplitter::handle { background:#DDE4EE; width:4px; margin:4px; }
QToolTip { background:#172033; color:#FFFFFF; border:0; padding:5px; }
)QSS");
}

inline QString dark() {
    return QStringLiteral(R"QSS(
QMainWindow, QWidget { background:#111722; color:#E7ECF5; font-family:"Microsoft YaHei UI"; font-size:13px; }
QWidget#centralPanel { background:#111722; }
QGroupBox { background:#192231; border:1px solid #303C50; border-radius:10px; margin-top:12px; padding:14px 10px 10px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; color:#D8E2F0; }
QLineEdit, QSpinBox, QPlainTextEdit, QTreeWidget { background:#151D2A; color:#E7ECF5; border:1px solid #3B485D; border-radius:7px; padding:6px 8px; selection-background-color:#4C82F7; selection-color:#FFFFFF; }
QLineEdit:focus, QSpinBox:focus, QPlainTextEdit:focus, QTreeWidget:focus { border:2px solid #6EA8FE; padding:5px 7px; }
QTreeWidget { alternate-background-color:#182130; outline:none; }
QTreeWidget::item { min-height:28px; border-radius:5px; }
QTreeWidget::item:hover { background:#25344A; }
QTreeWidget::item:selected { background:#294D84; color:#FFFFFF; }
QHeaderView::section { background:#202B3C; color:#C8D4E5; padding:7px; border:0; border-bottom:1px solid #344157; font-weight:600; }
QPushButton { background:#222D3D; color:#E7ECF5; border:1px solid #42516A; border-radius:7px; padding:7px 14px; min-height:18px; font-weight:600; }
QPushButton:hover { background:#2C3B50; border-color:#6EA8FE; color:#FFFFFF; }
QPushButton:pressed { background:#172B4D; border-color:#8DB9FF; color:#BFD8FF; }
QPushButton:checked { background:#3E7BDF; border-color:#76A9FA; color:#FFFFFF; }
QPushButton:disabled { background:#1A2230; color:#69778C; border-color:#2C3647; }
QPushButton#themeButton { background:#E9EEF7; color:#172033; border-color:#FFFFFF; }
QSplitter::handle { background:#303C50; width:4px; margin:4px; }
QToolTip { background:#E9EEF7; color:#172033; border:0; padding:5px; }
)QSS");
}

}  // namespace client_theme

#endif
