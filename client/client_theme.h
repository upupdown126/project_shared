#ifndef SMART_HOME_CLIENT_THEME_H
#define SMART_HOME_CLIENT_THEME_H

#include <QString>

namespace client_theme {

inline QString light() {
    return QStringLiteral(R"QSS(
QMainWindow, QWidget#centralPanel { background:#F3F6F9; color:#1F2937; }
QWidget { font-family:"Microsoft YaHei UI"; font-size:17px; }
QDialog { background:#F3F6F9; color:#1F2937; }
QDialog QLabel, QMessageBox QLabel, QInputDialog QLabel { color:#1F2937; }
QMenuBar { background:#FFFFFF; color:#344054; border-bottom:1px solid #E5EAF0; padding:5px 10px; }
QMenuBar::item { background:transparent; border-radius:5px; padding:8px 12px; }
QMenuBar::item:selected { background:#E8F2FC; color:#1269B0; }
QMenu { background:#FFFFFF; color:#344054; border:1px solid #D7E0E8; border-radius:8px; padding:6px; }
QMenu::item { border-radius:5px; padding:9px 32px 9px 14px; }
QMenu::item:selected { background:#E8F2FC; color:#1269B0; }
QMenu::separator { height:1px; background:#E5EAF0; margin:5px 8px; }
QToolBar { background:#FFFFFF; border:none; border-bottom:1px solid #E5EAF0; spacing:6px; padding:8px 12px; }
QToolBar::separator { width:1px; background:#D7E0E8; margin:5px 7px; }
QToolButton { background:transparent; color:#344054; border:1px solid transparent; border-radius:7px; padding:8px 12px; font-weight:600; }
QToolButton:hover { background:#E8F2FC; border-color:#DCEEFF; color:#1269B0; }
QToolButton:pressed, QToolButton:checked { background:#DCEEFF; color:#1269B0; border-color:#7CB8EB; }
QToolButton[role="primary"] { background:#1683E2; color:#FFFFFF; border-color:#1683E2; }
QToolButton[role="primary"]:hover { background:#0F74C9; color:#FFFFFF; border-color:#0F74C9; }
QToolButton[role="primary"]:pressed, QToolButton[role="primary"]:checked { background:#0C63AD; color:#FFFFFF; border-color:#0C63AD; }
QStatusBar { background:#FFFFFF; color:#667085; border-top:1px solid #DDE4EA; padding:3px 6px; }
QStatusBar::item { border:none; }
QDockWidget { color:#344054; font-weight:600; }
QDockWidget::title { background:#FFFFFF; border-bottom:1px solid #E5EAF0; padding:8px 12px; text-align:left; }
QFrame[card="true"] { background:#FFFFFF; border:1px solid #D7E0E8; border-radius:10px; }
QLabel { background:transparent; border:none; }
QLabel[role="appTitle"] { color:#1F2937; font-size:25px; font-weight:700; }
QLabel[role="title"] { color:#1F2937; font-size:20px; font-weight:700; }
QLabel[role="subtitle"] { color:#667085; font-size:17px; }
QLabel[role="field"] { color:#344054; font-size:17px; font-weight:600; }
QLabel[role="infoValue"] { color:#1F2937; font-size:17px; font-weight:600; }
QLabel[role="infoValue"][tone="neutral"] { color:#64748B; }
QLabel[role="infoValue"][tone="success"] { color:#16A34A; }
QLabel[role="infoValue"][tone="warning"] { color:#D97706; }
QLabel[role="infoValue"][tone="error"] { color:#DC2626; }
QLabel[role="badge"] { border-radius:13px; padding:6px 12px; font-size:17px; font-weight:600; border:1px solid transparent; }
QLabel[role="badge"][tone="neutral"] { background:#F1F5F9; color:#64748B; border-color:#E2E8F0; }
QLabel[role="badge"][tone="success"] { background:#E8F7EE; color:#15803D; border-color:#B7E4C7; }
QLabel[role="badge"][tone="warning"] { background:#FFF7E8; color:#D97706; border-color:#F7D9A4; }
QLabel[role="badge"][tone="info"] { background:#E6F7F9; color:#087B8F; border-color:#B8E3E8; }
QLabel[role="emptyIcon"] { color:#1683E2; font-size:25px; font-weight:700; }
QLabel[role="emptyTitle"] { color:#344054; font-size:20px; font-weight:700; }
QLabel[role="emptyHint"] { color:#667085; font-size:17px; }
QLabel[role="sliderEdge"] { color:#667085; font-size:17px; }
QLabel[role="sliderValue"] { background:#E4F2FF; color:#1269B0; border:1px solid #B8DDFC; border-radius:7px; padding:5px 8px; font-size:17px; font-weight:700; }
QWidget#deviceEmptyState { background:#EEF3F7; border:1px solid #E1E8EF; border-radius:8px; }
QStackedWidget#deviceContentStack { background:transparent; border:none; }
QLineEdit, QSpinBox, QPlainTextEdit, QTreeWidget {
    background:#FFFFFF; color:#344054; border:1px solid #CBD5E1; border-radius:7px;
    selection-background-color:#1683E2; selection-color:#FFFFFF;
}
QLineEdit, QSpinBox { min-height:32px; padding:7px 10px; }
QLineEdit:hover, QSpinBox:hover { border-color:#94A3B8; background:#FFFFFF; }
QLineEdit:focus, QSpinBox:focus, QPlainTextEdit:focus, QTreeWidget:focus { border:1px solid #1683E2; background:#FFFFFF; }
QLineEdit:disabled, QSpinBox:disabled { background:#F2F4F7; color:#98A2B3; }
QSpinBox::up-button, QSpinBox::down-button { width:24px; border:none; background:transparent; }
QCheckBox { color:#344054; spacing:7px; background:transparent; }
QCheckBox::indicator { width:20px; height:20px; border:1px solid #CBD5E1; border-radius:4px; background:#FFFFFF; }
QCheckBox::indicator:hover { border-color:#7CB8EB; }
QCheckBox::indicator:checked { background:#1683E2; border-color:#1683E2; image:none; }
QTreeWidget { padding:4px; outline:none; alternate-background-color:#F8FAFC; selection-background-color:#E4F2FF; selection-color:#1269B0; }
QTreeWidget::item { min-height:38px; border-radius:5px; padding:3px 7px; border-left:3px solid transparent; }
QTreeWidget::item:hover { background:#E8F2FC; }
QTreeWidget::item:selected { background:#E4F2FF; color:#1269B0; border-left:3px solid #1683E2; }
QHeaderView::section { background:#EEF3F7; color:#667085; padding:10px 11px; border:none; border-bottom:1px solid #D7E0E8; font-size:17px; font-weight:600; }
QPushButton { min-height:32px; background:#F7F9FC; color:#344054; border:1px solid #CCD6E0; border-radius:8px; padding:8px 14px; font-weight:600; }
QPushButton:hover { background:#E8F2FC; color:#1269B0; border-color:#7CB8EB; }
QPushButton:pressed { background:#DCEEFF; color:#1269B0; border-color:#1683E2; padding-top:10px; padding-bottom:6px; }
QPushButton:checked { background:#DCEEFF; color:#1269B0; border-color:#1683E2; }
QPushButton:disabled { background:#F2F4F7; color:#98A2B3; border-color:#E4E7EC; }
QPushButton[role="primary"] { background:#1683E2; color:#FFFFFF; border-color:#1683E2; }
QPushButton[role="primary"]:hover { background:#0F74C9; color:#FFFFFF; border-color:#0F74C9; }
QPushButton[role="primary"]:pressed, QPushButton[role="primary"]:checked { background:#0C63AD; color:#FFFFFF; border-color:#0C63AD; }
QPushButton[role="softPrimary"] { background:#E4F2FF; color:#1269B0; border-color:#B8DDFC; }
QPushButton[role="softPrimary"]:hover { background:#DCEEFF; border-color:#7CB8EB; }
QPushButton[role="danger"] { background:#FFF1F1; color:#C24141; border-color:#F3C0C0; }
QPushButton[role="danger"]:hover { background:#FDE7E7; border-color:#E89A9A; }
QPushButton[role="danger"]:pressed, QPushButton[role="danger"]:checked { background:#DC6262; color:#FFFFFF; border-color:#C94E4E; }
QPushButton[role="ptz"] { font-size:17px; padding:7px; }
QPushButton[role="ptz"][tone="stop"] { background:#FFF1F1; color:#C24141; border-color:#F3C0C0; }
QPushButton[role="ptz"][tone="stop"]:hover { background:#FDE7E7; border-color:#E89A9A; }
QPushButton[role="ptz"][tone="stop"]:pressed { background:#F8CCCC; color:#A52E2E; border-color:#D96F6F; }
QPushButton[role="layout"] { min-width:30px; min-height:30px; padding:6px 10px; font-size:17px; }
QPushButton#themeButton { background:#26313D; color:#F7F9FA; border-color:#26313D; }
QPushButton#themeButton:hover { background:#17212B; }
QTabWidget::pane { background:#FFFFFF; border:1px solid #E1E6EB; border-radius:10px; top:-1px; }
QTabBar::tab { background:#EDF1F4; color:#68747F; border:1px solid #E1E6EB; padding:7px 14px; margin-right:3px; border-top-left-radius:8px; border-top-right-radius:8px; }
QTabBar::tab:hover { background:#E7EEED; }
QTabBar::tab:selected { background:#FFFFFF; color:#315F5B; border-bottom-color:#FFFFFF; font-weight:600; }
QFrame#videoFrame { background:#E8EDF3; border:none; border-radius:9px; }
QStackedWidget#videoSurface { background:#080D14; border-radius:7px; }
QLabel#liveView { background:#080D14; border-radius:7px; color:#CBD5E1; }
QLabel#liveView[previewState="idle"] { background:#080D14; color:#CBD5E1; font-size:17px; }
QLabel#liveView[previewState="active"] { background:#080D14; color:#CBD5E1; }
QSlider::groove:horizontal { height:6px; background:#DCE6EF; border-radius:3px; }
QSlider::sub-page:horizontal { background:#1683E2; border-radius:3px; }
QSlider::add-page:horizontal { background:#DCE6EF; border-radius:3px; }
QSlider::handle:horizontal { width:18px; height:18px; margin:-6px 0; background:#FFFFFF; border:2px solid #1683E2; border-radius:9px; }
QSlider::handle:horizontal:hover { background:#E4F2FF; border-color:#0F74C9; }
QPlainTextEdit#logView { font-family:"Consolas", "Microsoft YaHei UI"; font-size:17px; padding:10px; }
QSplitter::handle { background:transparent; width:8px; height:8px; }
QSplitter::handle:hover { background:#D7E0E8; border-radius:3px; }
QScrollBar:vertical { background:transparent; width:10px; margin:2px; }
QScrollBar:horizontal { background:transparent; height:10px; margin:2px; }
QScrollBar::handle { background:#C7D2DC; border-radius:4px; min-height:28px; min-width:28px; }
QScrollBar::handle:hover { background:#A8B7C4; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
QToolTip { background:#26313D; color:#FFFFFF; border:none; padding:6px; }
)QSS");
}

inline QString dark() {
    return QStringLiteral(R"QSS(
QMainWindow, QWidget#centralPanel { background:#0E1319; color:#E6EBF0; }
QWidget { font-family:"Microsoft YaHei UI"; font-size:17px; }
QDialog { background:#0E1319; color:#E6EBF0; }
QDialog QLabel, QMessageBox QLabel, QInputDialog QLabel { color:#E6EDF3; }
QMenuBar { background:#171E26; color:#D5DDE4; border-bottom:1px solid #27323D; padding:5px 10px; }
QMenuBar::item { background:transparent; border-radius:5px; padding:8px 12px; }
QMenuBar::item:selected { background:#24332F; color:#B6D8D4; }
QMenu { background:#171E26; color:#D5DDE4; border:1px solid #34404B; border-radius:8px; padding:6px; }
QMenu::item { border-radius:5px; padding:9px 32px 9px 14px; }
QMenu::item:selected { background:#29413E; color:#D9EFEC; }
QMenu::separator { height:1px; background:#303B46; margin:5px 8px; }
QToolBar { background:#171E26; border:none; border-bottom:1px solid #27323D; spacing:6px; padding:8px 12px; }
QToolBar::separator { width:1px; background:#34404B; margin:5px 7px; }
QToolButton { background:transparent; color:#C7D0D8; border:1px solid transparent; border-radius:7px; padding:8px 12px; font-weight:600; }
QToolButton:hover { background:#23302F; border-color:#354A47; }
QToolButton:pressed, QToolButton:checked { background:#294642; color:#DDF2EE; border-color:#4F7773; }
QToolButton[role="primary"] { background:#4E8580; color:#FFFFFF; border-color:#4E8580; }
QToolButton[role="primary"]:hover { background:#5A928D; color:#FFFFFF; border-color:#5A928D; }
QToolButton[role="primary"]:pressed, QToolButton[role="primary"]:checked { background:#326965; color:#FFFFFF; border-color:#6BA09B; }
QStatusBar { background:#171E26; color:#99A5B0; border-top:1px solid #27323D; }
QStatusBar::item { border:none; }
QDockWidget { color:#D5DDE4; font-weight:600; }
QDockWidget::title { background:#171E26; border-bottom:1px solid #27323D; padding:8px 12px; text-align:left; }
QFrame[card="true"] { background:#171E26; border:1px solid #27323D; border-radius:12px; }
QLabel { background:transparent; border:none; }
QLabel[role="appTitle"] { color:#F0F4F7; font-size:25px; font-weight:700; }
QLabel[role="title"] { color:#E6EBF0; font-size:20px; font-weight:700; }
QLabel[role="subtitle"] { color:#8C99A6; font-size:17px; }
QLabel[role="field"] { color:#A4AFBA; font-size:17px; font-weight:600; }
QLabel[role="infoValue"] { color:#E6EDF3; font-size:17px; font-weight:600; }
QLabel[role="infoValue"][tone="neutral"] { color:#94A3B8; }
QLabel[role="infoValue"][tone="success"] { color:#34D399; }
QLabel[role="infoValue"][tone="warning"] { color:#F59E0B; }
QLabel[role="infoValue"][tone="error"] { color:#F87171; }
QLabel[role="badge"] { border-radius:13px; padding:6px 12px; font-size:17px; font-weight:600; }
QLabel[role="badge"][tone="neutral"] { background:#27313B; color:#AEB8C2; }
QLabel[role="badge"][tone="success"] { background:#193B31; color:#82C9AE; }
QLabel[role="badge"][tone="warning"] { background:#40331E; color:#E4BC70; }
QLabel[role="badge"][tone="info"] { background:#213743; color:#88B9CE; }
QLabel[role="emptyIcon"] { color:#64B5F6; font-size:25px; font-weight:700; }
QLabel[role="emptyTitle"] { color:#E6EBF0; font-size:20px; font-weight:700; }
QLabel[role="emptyHint"] { color:#8C99A6; font-size:17px; }
QLabel[role="sliderEdge"] { color:#8C99A6; font-size:17px; }
QLabel[role="sliderValue"] { background:#213743; color:#88B9CE; border:1px solid #365A6B; border-radius:7px; padding:5px 8px; font-size:17px; font-weight:700; }
QWidget#deviceEmptyState { background:#11171E; border:1px solid #2C3742; border-radius:8px; }
QStackedWidget#deviceContentStack { background:transparent; border:none; }
QLineEdit, QSpinBox, QPlainTextEdit, QTreeWidget {
    background:#11171E; color:#E2E8ED; border:1px solid #35414E; border-radius:8px;
    selection-background-color:#598985; selection-color:#FFFFFF;
}
QLineEdit, QSpinBox { min-height:32px; padding:7px 10px; }
QLineEdit:hover, QSpinBox:hover { border-color:#4D5D6B; background:#141B23; }
QLineEdit:focus, QSpinBox:focus, QPlainTextEdit:focus, QTreeWidget:focus { border:1px solid #72A39F; }
QLineEdit:disabled, QSpinBox:disabled { background:#151B22; color:#66727E; }
QSpinBox::up-button, QSpinBox::down-button { width:24px; border:none; background:transparent; }
QCheckBox { color:#AAB5BF; spacing:7px; background:transparent; }
QCheckBox::indicator { width:20px; height:20px; border:1px solid #4B5966; border-radius:4px; background:#11171E; }
QCheckBox::indicator:hover { border-color:#77A5A1; }
QCheckBox::indicator:checked { background:#568B86; border-color:#568B86; image:none; }
QTreeWidget { padding:4px; outline:none; alternate-background-color:#141B22; selection-background-color:#294642; selection-color:#DDF2EE; }
QTreeWidget::item { min-height:38px; border-radius:6px; padding:3px 7px; }
QTreeWidget::item:hover { background:#202C33; }
QTreeWidget::item:selected { background:#294642; color:#DDF2EE; }
QHeaderView::section { background:#1C252E; color:#98A5B1; padding:10px 11px; border:none; border-bottom:1px solid #2C3742; font-size:17px; font-weight:600; }
QPushButton { min-height:32px; background:#202933; color:#DCE3E9; border:1px solid #394653; border-radius:8px; padding:8px 14px; font-weight:600; }
QPushButton:hover { background:#28343F; border-color:#52616F; }
QPushButton:pressed { background:#182A2B; border-color:#6F9B97; padding-top:10px; padding-bottom:6px; }
QPushButton:checked { background:#294945; color:#DDF3EF; border-color:#6B9D98; }
QPushButton:disabled { background:#181E25; color:#65717C; border-color:#28313A; }
QPushButton[role="primary"] { background:#4E8580; color:#FFFFFF; border-color:#4E8580; }
QPushButton[role="primary"]:hover { background:#5A928D; border-color:#5A928D; }
QPushButton[role="primary"]:pressed, QPushButton[role="primary"]:checked { background:#326965; border-color:#6BA09B; }
QPushButton[role="softPrimary"] { background:#213743; color:#88B9CE; border-color:#365A6B; }
QPushButton[role="softPrimary"]:hover { background:#294754; border-color:#4D7283; }
QPushButton[role="danger"] { background:#342428; color:#E7A2A8; border-color:#60383E; }
QPushButton[role="danger"]:hover { background:#432B30; border-color:#895159; }
QPushButton[role="danger"]:pressed, QPushButton[role="danger"]:checked { background:#934B54; color:#FFFFFF; border-color:#B66A72; }
QPushButton[role="ptz"] { font-size:17px; padding:7px; }
QPushButton[role="ptz"][tone="stop"] { background:#342428; color:#E7A2A8; border-color:#60383E; }
QPushButton[role="ptz"][tone="stop"]:hover { background:#432B30; border-color:#895159; }
QPushButton[role="ptz"][tone="stop"]:pressed { background:#6E3940; color:#FFFFFF; border-color:#A95C65; }
QPushButton[role="layout"] { min-width:30px; min-height:30px; padding:6px 10px; font-size:17px; }
QPushButton#themeButton { background:#E5EBEF; color:#17212B; border-color:#E5EBEF; }
QPushButton#themeButton:hover { background:#FFFFFF; }
QTabWidget::pane { background:#171E26; border:1px solid #27323D; border-radius:10px; top:-1px; }
QTabBar::tab { background:#11171E; color:#8996A2; border:1px solid #27323D; padding:7px 14px; margin-right:3px; border-top-left-radius:8px; border-top-right-radius:8px; }
QTabBar::tab:hover { background:#202A34; }
QTabBar::tab:selected { background:#171E26; color:#A9D1CD; border-bottom-color:#171E26; font-weight:600; }
QFrame#videoFrame { background:#202A34; border:none; border-radius:9px; }
QStackedWidget#videoSurface { background:#070A0E; border-radius:7px; }
QLabel#liveView { background:#070A0E; border-radius:7px; color:#82909C; }
QLabel#liveView[previewState="idle"] { background:#111A22; color:#8FA0AD; font-size:17px; }
QLabel#liveView[previewState="active"] { background:#05080B; color:#82909C; }
QSlider::groove:horizontal { height:6px; background:#34414D; border-radius:3px; }
QSlider::sub-page:horizontal { background:#568B86; border-radius:3px; }
QSlider::add-page:horizontal { background:#34414D; border-radius:3px; }
QSlider::handle:horizontal { width:18px; height:18px; margin:-6px 0; background:#E5EBEF; border:2px solid #568B86; border-radius:9px; }
QSlider::handle:horizontal:hover { background:#FFFFFF; border-color:#72A39F; }
QPlainTextEdit#logView { font-family:"Consolas", "Microsoft YaHei UI"; font-size:17px; padding:10px; }
QSplitter::handle { background:transparent; width:8px; height:8px; }
QSplitter::handle:hover { background:#2A3740; border-radius:3px; }
QScrollBar:vertical { background:transparent; width:10px; margin:2px; }
QScrollBar:horizontal { background:transparent; height:10px; margin:2px; }
QScrollBar::handle { background:#3B4854; border-radius:4px; min-height:28px; min-width:28px; }
QScrollBar::handle:hover { background:#50606E; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
QToolTip { background:#E5EBEF; color:#17212B; border:none; padding:6px; }
)QSS");
}

}  // namespace client_theme

#endif
