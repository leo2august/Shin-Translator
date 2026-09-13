#include "mainwindow.h"
#include "module.h"
#include <winhttp.h>

extern const wchar_t* UPDATE_AVAILABLE;

int main(int argc, char *argv[])
{
	// VN Translator: pengecekan update Textractor dinonaktifkan (tidak relevan untuk fork ini).

	QDir::setCurrent(QFileInfo(S(GetModuleFilename().value())).absolutePath());

	QApplication app(argc, argv);
	app.setFont(QFont("Segoe UI", 10));

	// ===== Tema VN Translator (indigo/violet, off-white, dark navy) =====
	app.setStyleSheet(R"QSS(
		QWidget#simpleRoot, QWidget#stackRoot { background: #f7f7fb; }
		QWidget#appHeader { background: #1b2140; }
		QLabel#brand { color: #ffffff; font-size: 15px; font-weight: bold; }
		QLabel#headerStatus { color: #b9b9d6; font-size: 12px; }
		QLabel#engineBadge {
			color: #b9b9d6; font-size: 11px; font-weight: bold;
			background: rgba(255,255,255,0.08); border-radius: 6px; padding: 3px 9px;
		}
		QLabel#engineBadge[luna="true"] {
			color: #ffffff; background: rgba(124,108,255,0.45);
		}
		QPushButton#ghostBtn {
			color: #c7c7e6; background: rgba(255,255,255,0.08);
			border: none; border-radius: 6px; padding: 6px 12px;
		}
		QPushButton#ghostBtn:hover { background: rgba(255,255,255,0.18); color: #fff; }

		/* Tombol "Switch to LunaHook" di kartu terang -> harus kontras & terlihat */
		QPushButton#lunaSwitchBtn {
			color: #6a5fd6; background: #ece9ff; border: 1px solid #cfc6ff;
			border-radius: 8px; padding: 7px 14px; font-weight: bold;
		}
		QPushButton#lunaSwitchBtn:hover { background: #ddd8ff; border: 1px solid #7c6cff; }
		QPushButton#lunaSwitchBtn:pressed { background: #cfc6ff; }

		QScrollArea#bodyScroll, QWidget#bodyRoot { background: #f7f7fb; }
		QLabel#h1 { color: #1b2140; font-size: 20px; font-weight: bold; }
		QLabel#sub { color: #8a8aa0; font-size: 12px; }
		QLabel#section { color: #6a5fd6; font-size: 11px; font-weight: bold; margin-top: 6px; }

		QPushButton#card, QWidget#card {
			background: #ffffff; border: 1px solid #e6e6f0; border-radius: 12px;
			text-align: left;
		}
		QPushButton#card:hover { border: 1px solid #7c6cff; }
		QLabel#cardTitle { color: #1b2140; font-size: 14px; font-weight: bold; background: transparent; }
		QLabel#cardHint { color: #8a8aa0; font-size: 11px; background: transparent; }

		QWidget#previewCard { background: #ffffff; border: 1px solid #e6e6f0; border-radius: 12px; }
		QLabel#badge { color: #6a5fd6; font-size: 11px; font-weight: bold; background: transparent; }
		QLabel#srcText { color: #1b2140; font-size: 16px; background: transparent; }
		QLabel#dstText { color: #5a4fd6; font-size: 16px; font-weight: bold; background: transparent; }
		QLabel#statusText { color: #8a8aa0; font-size: 12px; }

		QPushButton#primaryBtn {
			background: #7c6cff; color: #ffffff; font-size: 15px; font-weight: bold;
			border: none; border-radius: 10px; padding: 14px;
		}
		QPushButton#primaryBtn:hover { background: #6a5be0; }
		QPushButton#primaryBtn:pressed { background: #5a4fd6; }

		/* Tombol "Save this game" + baris game tersimpan */
		QPushButton#saveGameBtn {
			background: #ece9ff; color: #6a5fd6; font-weight: bold;
			border: 1px solid #cfc6ff; border-radius: 10px; padding: 10px;
		}
		QPushButton#saveGameBtn:hover { background: #ddd8ff; border: 1px solid #7c6cff; }
		QWidget#savedGameRow { background: #ffffff; border: 1px solid #e6e6f0; border-radius: 10px; }
		QWidget#savedGameRow:hover { border: 1px solid #7c6cff; }
		QPushButton#miniBtn {
			background: #f4f4fb; color: #6a5fd6; border: 1px solid #e6e6f0;
			border-radius: 6px; font-weight: bold;
		}
		QPushButton#miniBtn:hover { background: #ece9ff; border: 1px solid #7c6cff; }
		QPushButton#miniDelBtn {
			background: rgba(255,90,120,0.12); color: #ef4444; border: none;
			border-radius: 6px; font-weight: bold;
		}
		QPushButton#miniDelBtn:hover { background: #ef4444; color: #ffffff; }

		/* ===== Mode Advanced dibuat lebih modern (beda dari Textractor) ===== */
		QWidget#advRoot { background: #f7f7fb; }
		QWidget#advInner { background: #f7f7fb; }
		QWidget#advHeader { background: #1b2140; }
		QLabel#advBrand { color: #ffffff; font-size: 14px; font-weight: bold; }
		QLabel#advModeBadge {
			color: #cfc9ff; background: rgba(124,108,255,0.28);
			border-radius: 6px; padding: 2px 10px; font-size: 11px; font-weight: bold;
		}
		QFrame#processFrame {
			background: #ffffff; border: 1px solid #e6e6f0; border-radius: 12px;
			margin: 10px 6px 10px 10px; padding: 6px;
		}
		QFrame#processFrame QPushButton {
			background: #f4f4fb; color: #1b2140; border: 1px solid #e6e6f0;
			border-radius: 8px; padding: 9px 12px; text-align: left; margin: 3px 2px;
		}
		QFrame#processFrame QPushButton:hover { background: #ece9ff; border: 1px solid #7c6cff; }
		QFrame#processFrame QPushButton:pressed { background: #ddd8ff; }
		QLabel#advSideSection {
			color: #9b8cff; font-size: 10px; font-weight: bold;
			margin: 8px 4px 2px 6px; letter-spacing: 1px;
		}
		QPushButton#advSidePrimary {
			background: #7c6cff; color: #ffffff; font-weight: bold; text-align: center;
			border: none; border-radius: 8px; padding: 9px 12px; margin: 6px 2px 2px 2px;
		}
		QPushButton#advSidePrimary:hover { background: #6a5be0; }
		QComboBox {
			background: #ffffff; color: #1b2140; border: 1px solid #e6e6f0;
			border-radius: 8px; padding: 7px 10px; margin: 4px 8px;
		}
		QComboBox:hover { border: 1px solid #7c6cff; }
		QComboBox QAbstractItemView {
			background: #ffffff; color: #1b2140; selection-background-color: #7c6cff;
			selection-color: #ffffff; border: 1px solid #e6e6f0;
		}
		QPlainTextEdit {
			background: #ffffff; color: #1b2140; border: 1px solid #e6e6f0;
			border-radius: 12px; padding: 12px; margin: 10px 10px 10px 6px;
			selection-background-color: #7c6cff; font-size: 15px;
		}
		QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
		QScrollBar::handle:vertical { background: #c9c9de; border-radius: 5px; min-height: 24px; }
		QScrollBar::handle:vertical:hover { background: #7c6cff; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
		QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
		QScrollBar::handle:horizontal { background: #c9c9de; border-radius: 5px; min-width: 24px; }
		QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

		/* ===== Panel Settings modern berkategori ===== */
		QDialog#settingsDialog { background: #0f1220; }
		QListWidget#settingsNav {
			background: #171b2e; color: #c7c7e6; border: none; outline: none;
			padding: 10px 6px; font-size: 13px;
		}
		QListWidget#settingsNav::item { padding: 10px 12px; border-radius: 8px; margin: 2px 6px; }
		QListWidget#settingsNav::item:selected { background: #7c6cff; color: #ffffff; }
		QListWidget#settingsNav::item:hover:!selected { background: rgba(124,108,255,0.18); }
		QStackedWidget#settingsPages { background: #0f1220; }
		QStackedWidget#settingsPages QScrollArea { background: #0f1220; border: none; }
		QStackedWidget#settingsPages QWidget { background: transparent; }
		QLabel#setHead { color: #ffffff; font-size: 20px; font-weight: bold; }
		QLabel#setSection { color: #9b8cff; font-size: 11px; font-weight: bold; margin-top: 6px; }
		QLabel#setLabel { color: #e7e7f5; font-size: 13px; }
		QLabel#setHint { color: #9a9ab8; font-size: 12px; }
		QWidget#setCard { background: #171b2e; border: 1px solid #262b45; border-radius: 12px; }
		QWidget#setCard QCheckBox { color: #e7e7f5; font-size: 13px; spacing: 8px; }
		QWidget#setCard QCheckBox::indicator { width: 16px; height: 16px; }
		QComboBox#setCombo {
			background: #0f1220; color: #ffffff; border: 1px solid #2f3454;
			border-radius: 8px; padding: 8px 10px; margin: 0;
		}
		QComboBox#setCombo:hover { border: 1px solid #7c6cff; }
		QComboBox#setCombo QAbstractItemView {
			background: #171b2e; color: #e7e7f5; selection-background-color: #7c6cff;
			selection-color: #ffffff; border: 1px solid #2f3454;
		}
		QWidget#setCard QSpinBox {
			background: #0f1220; color: #ffffff; border: 1px solid #2f3454;
			border-radius: 8px; padding: 5px 8px; min-width: 120px;
		}
		QWidget#setCard QComboBox {
			background: #0f1220; color: #ffffff; border: 1px solid #2f3454;
			border-radius: 8px; padding: 5px 8px; min-width: 120px;
		}
		QDialog#settingsDialog QPushButton {
			background: #7c6cff; color: #ffffff; font-weight: bold;
			border: none; border-radius: 10px; padding: 11px 18px;
		}
		QDialog#settingsDialog QPushButton:hover { background: #6a5be0; }
		QPushButton#setGhost {
			background: rgba(124,108,255,0.16); color: #cfc9ff; font-weight: normal;
			border: 1px solid #2f3454; border-radius: 8px; padding: 8px 14px;
		}
		QPushButton#setGhost:hover { background: rgba(124,108,255,0.3); }

		/* ===== Extension Manager (identitas sendiri) ===== */
		QWidget#extenRoot { background: #0f1220; }
		QWidget#extenHeader { background: #1b2140; }
		QLabel#extenTitle { color: #ffffff; font-size: 16px; font-weight: bold; }
		QLabel#extenSubtitle { color: #9a9ab8; font-size: 11px; }
		QPushButton#extenAdd {
			background: #7c6cff; color: #ffffff; font-weight: bold;
			border: none; border-radius: 8px; padding: 8px 16px;
		}
		QPushButton#extenAdd:hover { background: #6a5be0; }
		QListWidget#extenList {
			background: #0f1220; border: none; outline: none; padding: 8px;
		}
		QListWidget#extenList::item { background: transparent; border: none; }
		QListWidget#extenList::item:selected { background: transparent; }
		QWidget#extenRow {
			background: #171b2e; border: 1px solid #262b45; border-radius: 10px;
		}
		QWidget#extenRow:hover { border: 1px solid #7c6cff; }
		QLabel#extenGlyph {
			font-size: 15px; color: #cfc9ff;
			background: rgba(124,108,255,0.18); border-radius: 8px;
			min-width: 30px; min-height: 30px; max-width: 30px; max-height: 30px;
			qproperty-alignment: AlignCenter;
		}
		QLabel#extenName { color: #e7e7f5; font-size: 13px; font-weight: bold; background: transparent; }
		QLabel#extenGrip { color: #5a5f80; font-size: 14px; background: transparent; }
		QPushButton#extenDel {
			background: rgba(255,90,120,0.14); color: #ff7a95; font-weight: bold;
			border: none; border-radius: 6px;
		}
		QPushButton#extenDel:hover { background: #ff5a78; color: #ffffff; }
		QLabel#extenFooter { color: #6a6f90; font-size: 11px; padding: 8px 14px 12px 14px; background: #0f1220; }

		/* ===== Dictionary side panel (Japanese Learning) ===== */
		QWidget#dictPanel { background: #171b2e; border: 1px solid #2f3454; border-radius: 12px; }
		QLabel#dictHeader { color: #9b8cff; font-size: 12px; font-weight: bold; background: transparent; }
		QLabel#dictContent { color: #e7e7f5; font-size: 13px; background: transparent; }
		QPushButton#dictClose {
			background: rgba(255,255,255,0.06); color: #b9b9d6; border: none; border-radius: 6px; font-weight: bold;
		}
		QPushButton#dictClose:hover { background: rgba(255,90,120,0.5); color: #ffffff; }
		QPushButton#dictSave {
			background: #7c6cff; color: #ffffff; font-weight: bold;
			border: none; border-radius: 8px; padding: 9px;
		}
		QPushButton#dictSave:hover { background: #6a5be0; }
		/* teks asli klikabel: hover garis bawah halus */
		QLabel#srcText { color: #1b2140; }

		/* ===== Dialog pilih game (Attach) modern ===== */
		QDialog#attachDialog { background: #0f1220; }
		QWidget#attachHeader { background: #1b2140; }
		QLabel#attachTitle { color: #ffffff; font-size: 17px; font-weight: bold; }
		QLabel#attachSub { color: #9a9ab8; font-size: 12px; }
		QLineEdit#attachSearch {
			background: #0f1220; color: #ffffff; border: 1px solid #2f3454;
			border-radius: 9px; padding: 9px 12px; font-size: 13px;
		}
		QLineEdit#attachSearch:focus { border: 1px solid #7c6cff; }
		QListWidget#attachGrid {
			background: #0f1220; border: none; outline: none; padding: 12px;
		}
		QListWidget#attachGrid::item {
			background: #171b2e; border: 1px solid #262b45; border-radius: 12px;
			color: #e7e7f5; padding: 10px 6px;
		}
		QListWidget#attachGrid::item:hover { border: 1px solid #7c6cff; background: #1d2238; }
		QListWidget#attachGrid::item:selected { border: 1px solid #7c6cff; background: rgba(124,108,255,0.25); color: #ffffff; }
		QWidget#attachFooter { background: #141726; }
		QLabel#attachHint { color: #6a6f90; font-size: 11px; }
		QPushButton#attachStart {
			background: #7c6cff; color: #ffffff; font-weight: bold;
			border: none; border-radius: 10px; padding: 10px 22px;
		}
		QPushButton#attachStart:hover { background: #6a5be0; }
		QPushButton#attachCancel {
			background: rgba(255,255,255,0.06); color: #b9b9d6;
			border: 1px solid #2f3454; border-radius: 10px; padding: 10px 18px;
		}
		QPushButton#attachCancel:hover { background: rgba(255,255,255,0.12); color: #fff; }

		/* ===== Context menu (klik-kanan) agar selalu terbaca ===== */
		QMenu {
			background: #ffffff; color: #1b2140;
			border: 1px solid #e6e6f0; border-radius: 8px; padding: 4px;
		}
		QMenu::item { padding: 6px 22px 6px 14px; border-radius: 6px; }
		QMenu::item:selected { background: #ece9ff; color: #1b2140; }
		QMenu::item:disabled { color: #b0b0c4; }
		QMenu::separator { height: 1px; background: #e6e6f0; margin: 4px 8px; }
	)QSS");

	return MainWindow().show(), app.exec();
}
