#include "qtcommon.h"
#include "extension.h"
#include "ui_extrawindow.h"
#include "blockmarkup.h"
#include <fstream>
#include <process.h>
#include <QRegularExpression>
#include <QColorDialog>
#include <QFontDialog>
#include <QMenu>
#include <QPainter>
#include <QGraphicsEffect>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollArea>
#include <QAbstractNativeEventFilter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QHash>
#include <QSlider>
#include <QToolButton>
#include <QBoxLayout>
#include <QEvent>
#include <QScreen>

extern const char* EXTRA_WINDOW_INFO;
extern const char* TOPMOST;
extern const char* OPACITY;
extern const char* SHOW_ORIGINAL;
extern const char* ORIGINAL_AFTER_TRANSLATION;
extern const char* SIZE_LOCK;
extern const char* POSITION_LOCK;
extern const char* CENTERED_TEXT;
extern const char* AUTO_RESIZE_WINDOW_HEIGHT;
extern const char* CLICK_THROUGH;
extern const char* HIDE_MOUSEOVER;
extern const char* DICTIONARY;
extern const char* DICTIONARY_INSTRUCTIONS;
extern const char* BG_COLOR;
extern const char* TEXT_COLOR;
extern const char* TEXT_OUTLINE;
extern const char* OUTLINE_COLOR;
extern const char* OUTLINE_SIZE;
extern const char* OUTLINE_SIZE_INFO;
extern const char* FONT;

constexpr auto DICTIONARY_SAVE_FILE = u8"SavedDictionary.txt";
constexpr int CLICK_THROUGH_HOTKEY = 0xc0d0;

QColor colorPrompt(QWidget* parent, QColor default, const QString& title, bool customOpacity = true)
{
	QColor color = QColorDialog::getColor(default, parent, title);
	if (customOpacity) color.setAlpha(255 * QInputDialog::getDouble(parent, title, OPACITY, default.alpha() / 255.0, 0, 1, 3, nullptr, Qt::WindowCloseButtonHint));
	return color;
}

// --- Furigana ringan untuk overlay (Japanese Learning) ---
// Memuat jp_data/jlpt_vocab.json (kata -> bacaan kana) sekali, lalu memberi
// bacaan + spasi antar-kata pada teks asli Jepang. Tanpa tokenizer: pencocokan
// maju-terpanjang, hanya kata yang ada di kamus yang diberi furigana.
struct FuriganaHelper
{
	QHash<QString, QString> reading; // kata -> kana
	bool loaded = false;

	void EnsureLoaded()
	{
		if (loaded) return;
		loaded = true;
		QString path = QCoreApplication::applicationDirPath() + "/jp_data/jlpt_vocab.json";
		QFile f(path);
		if (!f.open(QIODevice::ReadOnly)) return;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
		f.close();
		if (!doc.isObject()) return;
		QJsonObject o = doc.object();
		for (auto it = o.begin(); it != o.end(); ++it)
		{
			QJsonArray arr = it.value().toArray();
			if (!arr.isEmpty()) reading.insert(it.key(), arr.first().toObject().value("reading").toString());
		}
	}

	static bool IsJp(QChar c)
	{
		ushort u = c.unicode();
		return (u >= 0x3040 && u <= 0x30ff) || (u >= 0x4e00 && u <= 0x9fff) || (u >= 0x3400 && u <= 0x4dbf);
	}

	// Ubah teks asli -> "kata(bacaan) kata2(bacaan2) ..." dengan spasi antar kata.
	QString Annotate(const QString& text, bool withFurigana)
	{
		EnsureLoaded();
		QString out;
		int i = 0, n = text.size();
		while (i < n)
		{
			QChar c = text.at(i);
			if (IsJp(c))
			{
				// pencocokan maju-terpanjang (maks 8) di kamus
				int best = 1;
				int maxLen = qMin(8, n - i);
				for (int len = maxLen; len >= 2; --len)
					if (reading.contains(text.mid(i, len))) { best = len; break; }
				QString seg = text.mid(i, best);
				out += seg;
				if (withFurigana && best >= 2)
				{
					auto it = reading.find(seg);
					if (it != reading.end() && !it.value().isEmpty() && it.value() != seg)
						out += "(" + it.value() + ")";
				}
				out += " "; // spasi antar kata
				i += best;
			}
			else { out += c; i += 1; }
		}
		return out.trimmed();
	}
} furiganaHelper;

struct PrettyWindow : QDialog, Localizer
{
	PrettyWindow(const char* name)
	{
		ui.setupUi(this);
		ui.display->setGraphicsEffect(outliner = new Outliner);
		setWindowFlags(Qt::FramelessWindowHint);
		setAttribute(Qt::WA_TranslucentBackground);

		settings.beginGroup(name);
		QFont font = ui.display->font();
		if (font.fromString(settings.value(FONT, font.toString()).toString())) ui.display->setFont(font);
		SetBackgroundColor(settings.value(BG_COLOR, backgroundColor).value<QColor>());
		SetTextColor(settings.value(TEXT_COLOR, TextColor()).value<QColor>());
		// Default: outline tipis GELAP menyala (backing opak) supaya teks tetap TAJAM
		// & terbaca walau background overlay dibuat transparan (mengatasi teks pudar).
		// Pengguna tetap bisa ubah/matikan lewat menu klik-kanan.
		outliner->color = settings.value(OUTLINE_COLOR, QColor(0, 0, 0, 220)).value<QColor>();
		outliner->size = settings.value(OUTLINE_SIZE, 0.6).toDouble();
		// Persist default outline bila belum pernah tersimpan, supaya setting outline
		// bertahan antar sesi (tidak "lepas" setiap overlay dibuka lagi).
		if (!settings.contains(OUTLINE_COLOR)) settings.setValue(OUTLINE_COLOR, outliner->color.name(QColor::HexArgb));
		if (!settings.contains(OUTLINE_SIZE)) settings.setValue(OUTLINE_SIZE, outliner->size);
		autoHide = settings.value(HIDE_MOUSEOVER, autoHide).toBool();
		menu.addAction(FONT, this, &PrettyWindow::RequestFont);
		menu.addAction(BG_COLOR, [this] { SetBackgroundColor(colorPrompt(this, backgroundColor, BG_COLOR)); });
		menu.addAction(TEXT_COLOR, [this] { SetTextColor(colorPrompt(this, TextColor(), TEXT_COLOR)); });
		QAction* outlineAction = menu.addAction(TEXT_OUTLINE, this, &PrettyWindow::SetOutline);
		outlineAction->setCheckable(true);
		outlineAction->setChecked(outliner->size >= 0);
		QAction* autoHideAction = menu.addAction(HIDE_MOUSEOVER, this, [this](bool autoHide) { settings.setValue(HIDE_MOUSEOVER, this->autoHide = autoHide); });
		autoHideAction->setCheckable(true);
		autoHideAction->setChecked(autoHide);
		connect(this, &QDialog::customContextMenuRequested, [this](QPoint point) { menu.exec(mapToGlobal(point)); });
		connect(ui.display, &QLabel::customContextMenuRequested, [this](QPoint point) { menu.exec(ui.display->mapToGlobal(point)); });
		startTimer(50);
	}

	~PrettyWindow()
	{
		settings.sync();
	}

	Ui::ExtraWindow ui;

	// Atur opacity keseluruhan overlay (background + teks + outline) dari 0..255.
	// Dipakai slider transparansi. Menyimpan warna dasar agar rasio antar elemen terjaga.
	void ApplyOverlayAlpha(int alpha)
	{
		if (alpha < 3) alpha = 3; if (alpha > 255) alpha = 255;
		// Simpan HANYA sebagai alpha khusus untuk cat background (paintEvent).
		// TIDAK menyentuh warna teks/outline sama sekali -> teks selalu solid.
		bgPaintAlpha = alpha;
		repaint();
	}
	// Warna background efektif untuk dicat: warna dasar dgn alpha slider (0..255).
	QColor OverlayBackgroundColor() const
	{
		QColor c = backgroundColor;
		c.setAlpha(bgPaintAlpha >= 0 ? bgPaintAlpha : backgroundColor.alpha());
		return c;
	}
	int bgPaintAlpha = -1; // -1 = pakai alpha bawaan backgroundColor
	// Warna asli yang disimpan saat auto-hide fade, agar bisa dipulihkan persis.
	QColor savedBgColor, savedOutlineColor, savedTextColor;

protected:
	void timerEvent(QTimerEvent*) override
	{
		if (autoHide && geometry().contains(QCursor::pos()))
		{
			if (!hidden)
			{
				// Simpan warna ASLI sebelum difade, agar dipulihkan persis (jangan
				// bergantung ke QSettings yang mungkin belum tersimpan -> outline hilang).
				savedBgColor = backgroundColor;
				savedOutlineColor = outliner->color;
				savedTextColor = TextColor();
				if (backgroundColor.alphaF() > 0.05) backgroundColor.setAlphaF(0.05);
				if (outliner->color.alphaF() > 0.05) outliner->color.setAlphaF(0.05);
				QColor hiddenTextColor = savedTextColor;
				if (hiddenTextColor.alphaF() > 0.05) hiddenTextColor.setAlphaF(0.05);
				ui.display->setPalette(QPalette(hiddenTextColor, {}, {}, {}, {}, {}, {}));
				hidden = true;
				repaint();
			}
		}
		else if (hidden)
		{
			// Pulihkan dari nilai yang disimpan saat fade (bukan dari settings).
			backgroundColor = savedBgColor;
			outliner->color = savedOutlineColor;
			ui.display->setPalette(QPalette(savedTextColor, {}, {}, {}, {}, {}, {}));
			hidden = false;
			repaint();
		}
	}

	QMenu menu{ ui.display };
	Settings settings{ this };

private:
	void RequestFont()
	{
		if (QFont font = QFontDialog::getFont(&ok, ui.display->font(), this, FONT); ok)
		{
			settings.setValue(FONT, font.toString());
			ui.display->setFont(font);
		}
	};

	void SetBackgroundColor(QColor color)
	{
		if (!color.isValid()) return;
		if (color.alpha() == 0) color.setAlpha(1);
		backgroundColor = color;
		repaint();
		settings.setValue(BG_COLOR, color.name(QColor::HexArgb));
	};

	QColor TextColor()
	{
		return ui.display->palette().color(QPalette::WindowText);
	}

	void SetTextColor(QColor color)
	{
		if (!color.isValid()) return;
		ui.display->setPalette(QPalette(color, {}, {}, {}, {}, {}, {}));
		settings.setValue(TEXT_COLOR, color.name(QColor::HexArgb));
	};

	void SetOutline(bool enable)
	{
		if (enable)
		{
			QColor color = colorPrompt(this, outliner->color, OUTLINE_COLOR);
			if (color.isValid()) outliner->color = color;
			outliner->size = QInputDialog::getDouble(this, OUTLINE_SIZE, OUTLINE_SIZE_INFO, -outliner->size, 0, INT_MAX, 2, nullptr, Qt::WindowCloseButtonHint);
		}
		else outliner->size = -outliner->size;
		settings.setValue(OUTLINE_COLOR, outliner->color.name(QColor::HexArgb));
		settings.setValue(OUTLINE_SIZE, outliner->size);
	}

	void paintEvent(QPaintEvent*) override
	{
		QPainter(this).fillRect(rect(), backgroundColor);
	}

	bool autoHide = false, hidden = false;
	QColor backgroundColor{ palette().window().color() };
	struct Outliner : QGraphicsEffect
	{
		void draw(QPainter* painter) override
		{
			if (size < 0) return drawSource(painter);
			QPoint offset;
			QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset);
			offset.setX(offset.x() + size);
			for (auto offset2 : Array<QPointF>{ { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } })
			{
				QImage outline = pixmap.toImage();
				QPainter outlinePainter(&outline);
				outlinePainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
				outlinePainter.fillRect(outline.rect(), color);
				painter->drawImage(offset + offset2 * size, outline);
			}
			painter->drawPixmap(offset, pixmap);
		}
		QColor color{ Qt::black };
		double size = -0.5;
	}* outliner;
};

class ExtraWindow : public PrettyWindow, QAbstractNativeEventFilter
{
public:
	ExtraWindow() : PrettyWindow("Extra Window")
	{
		ui.display->setTextFormat(Qt::PlainText);
		// Ukuran minimum wajar supaya overlay tidak pernah ciut jadi sangat kecil.
		setMinimumSize(280, 90);
		if (settings.contains(WINDOW) && QApplication::screenAt(settings.value(WINDOW).toRect().bottomRight()))
			setGeometry(settings.value(WINDOW).toRect());
		else
		{
			// Pertama kali: posisikan di bawah-tengah layar dengan ukuran tetap yang enak.
			QRect scr = QApplication::primaryScreen()->availableGeometry();
			int w = qMin(760, scr.width() - 80), h = 150;
			setGeometry(scr.center().x() - w / 2, scr.bottom() - h - 60, w, h);
		}

		for (auto [name, default, slot] : Array<const char*, bool, void(ExtraWindow::*)(bool)>{
			// VN Translator: default overlay siap-pakai (topmost + auto-resize, hanya terjemahan)
			{ TOPMOST, true, &ExtraWindow::SetTopmost },
			{ SIZE_LOCK, false, &ExtraWindow::SetSizeLock },
			{ POSITION_LOCK, false, &ExtraWindow::SetPositionLock },
			{ CENTERED_TEXT, false, &ExtraWindow::SetCenteredText },
			{ AUTO_RESIZE_WINDOW_HEIGHT, true, &ExtraWindow::SetAutoResize },
			{ SHOW_ORIGINAL, false, &ExtraWindow::SetShowOriginal },
			{ ORIGINAL_AFTER_TRANSLATION, true, &ExtraWindow::SetShowOriginalAfterTranslation },
			{ DICTIONARY, false, &ExtraWindow::SetUseDictionary },
		})
		{
			// delay processing anything until Textractor has finished initializing
			QMetaObject::invokeMethod(this, std::bind(slot, this, default = settings.value(name, default).toBool()), Qt::QueuedConnection);
			auto action = menu.addAction(name, this, slot);
			action->setCheckable(true);
			action->setChecked(default);
		}

		menu.addAction(CLICK_THROUGH, this, &ExtraWindow::ToggleClickThrough);

		// Japanese Learning: opsi tampilkan furigana & spasi antar kata pada teks asli.
		{
			auto furiAction = menu.addAction(QString::fromUtf8(u8"Furigana on original (JP)"), this, [this](bool on)
			{ settings.setValue("Furigana original", furiganaOriginal = on); DisplaySentence(); });
			furiAction->setCheckable(true);
			furiganaOriginal = settings.value("Furigana original", false).toBool();
			furiAction->setChecked(furiganaOriginal);

			auto spaceAction = menu.addAction(QString::fromUtf8(u8"Space between words (JP)"), this, [this](bool on)
			{ settings.setValue("Space words", spaceWords = on); DisplaySentence(); });
			spaceAction->setCheckable(true);
			spaceWords = settings.value("Space words", false).toBool();
			spaceAction->setChecked(spaceWords);

			auto showImmAction = menu.addAction(QString::fromUtf8(u8"Show overlay immediately"), this, [this](bool on)
			{ settings.setValue("Show immediately", showImmediately = on); if (on && !isVisible()) show(); });
			showImmAction->setCheckable(true);
			showImmediately = settings.value("Show immediately", true).toBool();
			showImmAction->setChecked(showImmediately);
		}

		ui.display->installEventFilter(this);
		qApp->installNativeEventFilter(this);

		QMetaObject::invokeMethod(this, [this]
		{
			RegisterHotKey((HWND)winId(), CLICK_THROUGH_HOTKEY, MOD_ALT | MOD_NOREPEAT, 0x58);
			// VN Translator: tampilkan overlay langsung di awal (default) dengan
			// placeholder, supaya pengguna tahu overlay sudah siap tanpa menunggu
			// teks pertama. Bisa dimatikan lewat "Show overlay immediately".
			if (showImmediately)
			{
				ui.display->setText(QString::fromUtf8(u8"Shin Translator \u2014 waiting for text\u2026"));
				show();
			}
			else hide();
		}, Qt::QueuedConnection);

		BuildChrome(); // toolbar navigasi + slider transparansi
		// Terapkan opacity tersimpan.
		QMetaObject::invokeMethod(this, [this] { SetOverlayOpacity(OverlayOpacity()); }, Qt::QueuedConnection);
	}

	~ExtraWindow()
	{
		settings.setValue(WINDOW, geometry());
	}

	void AddSentence(QString sentence)
	{
		sanitize(sentence);
		sentence.chop(std::distance(std::remove(sentence.begin(), sentence.end(), QChar::Tabulation), sentence.end()));
		sentenceHistory.push_back(sentence);
		if (sentenceHistory.size() > 1000) sentenceHistory.erase(sentenceHistory.begin());
		historyIndex = sentenceHistory.size() - 1;
		if (minimized) return; // saat pill: simpan teks, jangan tampil/resize sampai di-restore
		DisplaySentence();
		// VN Translator: tampilkan overlay begitu ada teks pertama.
		if (!isVisible()) show();
	}

private:
	void DisplaySentence()
	{
		if (sentenceHistory.empty()) return;
		QString sentence = sentenceHistory[historyIndex];
		if (sentence.contains(u8"\x200b \n"))
		{
			QString original = sentence.split(u8"\x200b \n")[0];
			QString translation = sentence.split(u8"\x200b \n")[1];
			// Japanese Learning: beri furigana + spasi antar kata pada teks asli.
			if ((furiganaOriginal || spaceWords) && !original.isEmpty())
				original = furiganaHelper.Annotate(original, furiganaOriginal);
			if (!showOriginal) sentence = translation;
			else if (showOriginalAfterTranslation) sentence = translation + "\n" + original;
			else sentence = original + "\n" + translation;
		}

		if (sizeLock && !autoResize)
		{
			QFontMetrics fontMetrics(ui.display->font(), ui.display);
			int low = 0, high = sentence.size(), last = 0;
			while (low <= high)
			{
				int mid = (low + high) / 2;
				if (fontMetrics.boundingRect(0, 0, ui.display->width(), INT_MAX, Qt::TextWordWrap, sentence.left(mid)).height() <= ui.display->height())
				{
					last = mid;
					low = mid + 1;
				}
				else high = mid - 1;
			}
			sentence = sentence.left(last);
		}

		ui.display->setText(sentence);
		if (autoResize)
		{
			int newH = height() - ui.display->height() +
				QFontMetrics(ui.display->font(), ui.display).boundingRect(0, 0, ui.display->width(), INT_MAX, Qt::TextWordWrap, sentence).height();
			if (newH < minimumHeight()) newH = minimumHeight(); // jangan ciut di bawah minimum
			resize(width(), newH);
		}
	}

	void SetTopmost(bool topmost)
	{
		for (auto window : { winId(), dictionaryWindow.winId() })
			SetWindowPos((HWND)window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		settings.setValue(TOPMOST, topmost);
	};

	void SetPositionLock(bool locked)
	{
		settings.setValue(POSITION_LOCK, posLock = locked);
	};

	void SetSizeLock(bool locked)
	{
		setSizeGripEnabled(!locked);
		settings.setValue(SIZE_LOCK, sizeLock = locked);
	};

	void SetCenteredText(bool centeredText)
	{
		ui.display->setAlignment(centeredText ? Qt::AlignHCenter : Qt::AlignLeft);
		settings.setValue(CENTERED_TEXT, this->centeredText = centeredText);
	};

	void SetAutoResize(bool autoResize)
	{
		settings.setValue(AUTO_RESIZE_WINDOW_HEIGHT, this->autoResize = autoResize);
		DisplaySentence();
	};

	void SetShowOriginal(bool showOriginal)
	{
		settings.setValue(SHOW_ORIGINAL, this->showOriginal = showOriginal);
		DisplaySentence();
	};

	void SetShowOriginalAfterTranslation(bool showOriginalAfterTranslation)
	{
		settings.setValue(ORIGINAL_AFTER_TRANSLATION, this->showOriginalAfterTranslation = showOriginalAfterTranslation);
		DisplaySentence();
	};

	void SetUseDictionary(bool useDictionary)
	{
		if (useDictionary)
		{
			dictionaryWindow.UpdateDictionary();
			if (dictionaryWindow.dictionary.empty())
			{
				std::ofstream(DICTIONARY_SAVE_FILE) << u8"\ufeff" << DICTIONARY_INSTRUCTIONS;
				_spawnlp(_P_DETACH, "notepad", "notepad", DICTIONARY_SAVE_FILE, NULL); // show file to user
			}
		}
		settings.setValue(DICTIONARY, this->useDictionary = useDictionary);
	}

	void ToggleClickThrough()
	{
		clickThrough = !clickThrough;
		for (auto window : { winId(), dictionaryWindow.winId() })
		{
			unsigned exStyle = GetWindowLongPtrW((HWND)window, GWL_EXSTYLE);
			if (clickThrough) exStyle |= WS_EX_TRANSPARENT;
			else exStyle &= ~WS_EX_TRANSPARENT;
			SetWindowLongPtrW((HWND)window, GWL_EXSTYLE, exStyle);
		}
		// Penanda ON + cara mematikan (Alt+X) selalu terlihat saat aktif,
		// karena saat click-through mouse tak bisa dipakai untuk mematikannya.
		if (clickBtn) clickBtn->setChecked(clickThrough);
		if (clickThroughBadge)
		{
			clickThroughBadge->setVisible(clickThrough);
			if (clickThrough) { LayoutChrome(); clickThroughBadge->raise(); }
		}
	};

	void ShowDictionary(QPoint mouse)
	{
		QString sentence = ui.display->text();
		const QFont& font = ui.display->font();
		if (cachedDisplayInfo.CompareExchange(ui.display))
		{
			QFontMetrics fontMetrics(font, ui.display);
			int flags = Qt::TextWordWrap | (ui.display->alignment() & (Qt::AlignLeft | Qt::AlignHCenter));
			textPositionMap.clear();
			for (int i = 0, height = 0, lineBreak = 0; i < sentence.size(); ++i)
			{
				int block = 1;
				for (int charHeight = fontMetrics.boundingRect(0, 0, 1, INT_MAX, flags, sentence.mid(i, 1)).height();
					i + block < sentence.size() && fontMetrics.boundingRect(0, 0, 1, INT_MAX, flags, sentence.mid(i, block + 1)).height() < charHeight * 1.5; ++block);
				auto boundingRect = fontMetrics.boundingRect(0, 0, ui.display->width(), INT_MAX, flags, sentence.left(i + block));
				if (boundingRect.height() > height)
				{
					height = boundingRect.height();
					lineBreak = i;
				}
				textPositionMap.push_back({
					fontMetrics.boundingRect(0, 0, ui.display->width(), INT_MAX, flags, sentence.mid(lineBreak, i - lineBreak + 1)).right() + 1,
					height
				});
			}
		}
		int i;
		for (i = 0; i < textPositionMap.size(); ++i) if (textPositionMap[i].y() > mouse.y() && textPositionMap[i].x() > mouse.x()) break;
		if (i == textPositionMap.size() || (mouse - textPositionMap[i]).manhattanLength() > font.pointSize() * 3) return dictionaryWindow.hide();
		if (sentence.mid(i) == dictionaryWindow.term) return dictionaryWindow.ShowDefinition();
		dictionaryWindow.ui.display->setFixedWidth(ui.display->width() * 3 / 4);
		dictionaryWindow.SetTerm(sentence.mid(i));
		int left = i == 0 ? 0 : textPositionMap[i - 1].x(), right = textPositionMap[i].x(),
			x = textPositionMap[i].x() > ui.display->width() / 2 ? -dictionaryWindow.width() + (right * 3 + left) / 4 : (left * 3 + right) / 4, y = 0;
		for (auto point : textPositionMap) if (point.y() > y && point.y() < textPositionMap[i].y()) y = point.y();
		dictionaryWindow.move(ui.display->mapToGlobal(QPoint(x, y - dictionaryWindow.height())));
	}

	bool nativeEventFilter(const QByteArray&, void* message, long* result) override
	{
		auto msg = (MSG*)message;
		if (msg->message == WM_HOTKEY)
			if (msg->wParam == CLICK_THROUGH_HOTKEY) return ToggleClickThrough(), true;
		return false;
	}

	bool eventFilter(QObject*, QEvent* event) override
	{
		if (event->type() == QEvent::MouseButtonPress) mousePressEvent((QMouseEvent*)event);
		return false;
	}

	void timerEvent(QTimerEvent* event) override
	{
		if (useDictionary && QCursor::pos() != oldPos && (!dictionaryWindow.isVisible() || !dictionaryWindow.geometry().contains(QCursor::pos())))
			ShowDictionary(ui.display->mapFromGlobal(QCursor::pos()));

		// Tampilkan chrome (toolbar+slider) bila kursor di atas overlay ATAU slider,
		// dengan margin sedikit di sekitar keduanya. Sembunyikan bila keluar semuanya.
		// Berbasis polling supaya andal walau slider adalah jendela terpisah.
		if (!minimized)
		{
			QPoint c = QCursor::pos();
			QRect ov = frameGeometry().adjusted(-12, -12, 12, 12);
			bool overOverlay = ov.contains(c);
			bool overSlider = false;
			if (auto sb = SliderBox(); sb && sb->isVisible())
				overSlider = sb->frameGeometry().adjusted(-12, -12, 12, 12).contains(c);
			bool want = overOverlay || overSlider;
			bool shown = toolbar && toolbar->isVisible();
			if (want && !shown) ShowChrome(true);
			else if (!want && shown) ShowChrome(false);
		}
		PrettyWindow::timerEvent(event);
	}

	void mousePressEvent(QMouseEvent* event) override
	{
		if (minimized) { ToggleMinimize(); return; } // klik pill -> restore
		dictionaryWindow.hide();
		oldPos = event->globalPos();
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		if (minimized) return; // pill: jangan dipindah lewat drag isi
		if (!posLock) move(pos() + event->globalPos() - oldPos);
		oldPos = event->globalPos();
	}

	// Tampil/sembunyi chrome ditangani via timerEvent (polling) agar andal
	// walau slider adalah jendela terpisah di luar overlay.

	void resizeEvent(QResizeEvent* e) override
	{
		QDialog::resizeEvent(e);
		LayoutChrome();
	}

	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		if (minimized)
		{
			// gambar pill bulat dengan aksen ungu + ikon.
			p.setRenderHint(QPainter::Antialiasing);
			p.setBrush(QColor(0x7c, 0x6c, 0xff));
			p.setPen(Qt::NoPen);
			p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 12, 12);
			p.setPen(QColor(255, 255, 255));
			QFont f = p.font(); f.setPointSize(16); f.setBold(true); p.setFont(f);
			p.drawText(rect(), Qt::AlignCenter, QString::fromUtf8(u8"\u2726")); // bintang kecil
			return;
		}
		// Normal: isi latar dengan warna background overlay (sama seperti PrettyWindow).
		p.fillRect(rect(), OverlayBackgroundColor());
	}

	void wheelEvent(QWheelEvent* event) override
	{
		int scroll = event->angleDelta().y();
		if (scroll > 0 && historyIndex > 0) --historyIndex;
		if (scroll < 0 && historyIndex + 1 < sentenceHistory.size()) ++historyIndex;
		DisplaySentence();
	}

	// ================= Redesign: toolbar + slider + minimize =================

	QToolButton* MakeToolBtn(const QString& glyph, const QString& tip)
	{
		auto b = new QToolButton(toolbar);
		b->setText(glyph);
		b->setToolTip(tip);
		b->setCursor(Qt::PointingHandCursor);
		b->setFixedSize(28, 28);
		b->setFocusPolicy(Qt::NoFocus);
		b->setStyleSheet(
			"QToolButton { color:#e7e7f5; background:rgba(27,33,64,0.85); border:none;"
			" border-radius:8px; font-size:14px; }"
			"QToolButton:hover { background:#7c6cff; color:#ffffff; }"
			"QToolButton:checked { background:#7c6cff; color:#ffffff; }");
		return b;
	}

	void BuildChrome()
	{
		// --- Toolbar navigasi (mengambang di kanan-atas overlay) ---
		toolbar = new QWidget(this);
		toolbar->setObjectName("ovToolbar");
		toolbar->setStyleSheet("QWidget#ovToolbar { background:rgba(15,18,32,0.72); border-radius:12px; }");
		auto tl = new QHBoxLayout(toolbar);
		tl->setContentsMargins(6, 5, 6, 5); tl->setSpacing(4);

		auto bSettings = MakeToolBtn(QString::fromUtf8(u8"\u2699"), "Settings (all options)");
		connect(bSettings, &QToolButton::clicked, [this] { menu.exec(mapToGlobal(QPoint(width() / 2, 40))); });
		tl->addWidget(bSettings);

		pinBtn = MakeToolBtn(QString::fromUtf8(u8"\U0001F4CC"), "Keep on top");
		pinBtn->setCheckable(true); pinBtn->setChecked(settings.value(TOPMOST, true).toBool());
		connect(pinBtn, &QToolButton::clicked, [this](bool on) { SetTopmost(on); });
		tl->addWidget(pinBtn);

		lockBtn = MakeToolBtn(QString::fromUtf8(u8"\U0001F512"), "Lock position");
		lockBtn->setCheckable(true); lockBtn->setChecked(settings.value(POSITION_LOCK, false).toBool());
		connect(lockBtn, &QToolButton::clicked, [this](bool on) { SetPositionLock(on); });
		tl->addWidget(lockBtn);

		clickBtn = MakeToolBtn(QString::fromUtf8(u8"\U0001F446"), "Click-through (mouse passes through)");
		clickBtn->setCheckable(true);
		connect(clickBtn, &QToolButton::clicked, [this] { ToggleClickThrough(); });
		tl->addWidget(clickBtn);

		auto bMin = MakeToolBtn(QString::fromUtf8(u8"\u2013"), "Minimize to a small bubble");
		connect(bMin, &QToolButton::clicked, [this] { ToggleMinimize(); });
		tl->addWidget(bMin);

		auto bClose = MakeToolBtn(QString::fromUtf8(u8"\u2715"), "Hide overlay (click the bubble to bring it back)");
		bClose->setStyleSheet(bClose->styleSheet() + "QToolButton:hover { background:#ef4444; color:#fff; }");
		connect(bClose, &QToolButton::clicked, [this] { minimized = false; ToggleMinimize(); });
		tl->addWidget(bClose);

		toolbar->hide();

		// --- Handle drag di header (penanda area geser posisi) ---
		dragHandle = new QWidget(this);
		dragHandle->setObjectName("ovDrag");
		dragHandle->setToolTip("Drag here to move the overlay");
		dragHandle->setCursor(Qt::SizeAllCursor);
		dragHandle->setFixedHeight(16);
		dragHandle->setStyleSheet(
			"QWidget#ovDrag { background:rgba(124,108,255,0.22); border-top-left-radius:8px;"
			" border-top-right-radius:8px; }");
		auto dhl = new QHBoxLayout(dragHandle); dhl->setContentsMargins(0, 0, 0, 0);
		auto grip = new QLabel(QString::fromUtf8(u8"\u2022 \u2022 \u2022 \u2022 \u2022"), dragHandle);
		grip->setAlignment(Qt::AlignCenter);
		grip->setStyleSheet("color:rgba(207,201,255,0.8); background:transparent; font-size:10px; letter-spacing:2px;");
		dhl->addWidget(grip);
		dragHandle->hide();

		// --- Penanda Click-through ON (dengan cara mematikan) ---
		clickThroughBadge = new QLabel(this);
		clickThroughBadge->setObjectName("ovClickBadge");
		clickThroughBadge->setText(QString::fromUtf8(u8"\U0001F446 Click-through ON \u2014 press Alt+X to turn off"));
		clickThroughBadge->setStyleSheet(
			"QLabel#ovClickBadge { color:#ffffff; background:rgba(124,108,255,0.9);"
			" border-radius:8px; padding:4px 10px; font-size:11px; font-weight:bold; }");
		clickThroughBadge->hide();

		// --- Slider transparansi vertikal (gaya volume) sebagai JENDELA TERPISAH,
		//     didok di LUAR tepi kanan overlay supaya tidak menutupi toolbar/tombol X. ---
		auto sliderBox = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput);
		sliderBox->setAttribute(Qt::WA_TranslucentBackground);
		sliderBox->setAttribute(Qt::WA_ShowWithoutActivating);
		// Slider tetap bisa di-drag: hapus TransparentForInput (kita ingin interaktif).
		sliderBox->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
		sliderBox->setObjectName("ovSliderBox");
		sliderBox->setStyleSheet("QWidget#ovSliderBox { background:rgba(15,18,32,0.9);"
			" border-radius:12px; }");
		auto sv = new QVBoxLayout(sliderBox);
		sv->setContentsMargins(5, 8, 5, 8); sv->setSpacing(6);
		auto icon = new QLabel(QString::fromUtf8(u8"\U0001F4A7"), sliderBox); // tetes -> transparansi
		icon->setAlignment(Qt::AlignHCenter); icon->setStyleSheet("color:#cfc9ff; background:transparent; font-size:13px;");
		sv->addWidget(icon);
		opacitySlider = new QSlider(Qt::Vertical, sliderBox);
		opacitySlider->setRange(10, 100);
		opacitySlider->setValue(OverlayOpacity());
		opacitySlider->setFixedHeight(120);
		opacitySlider->setCursor(Qt::PointingHandCursor);
		opacitySlider->setStyleSheet(
			"QSlider::groove:vertical { width:6px; background:rgba(255,255,255,0.15); border-radius:3px; }"
			"QSlider::handle:vertical { height:16px; margin:0 -6px; background:#7c6cff; border-radius:8px; }"
			"QSlider::sub-page:vertical { background:rgba(255,255,255,0.15); border-radius:3px; }"
			"QSlider::add-page:vertical { background:#7c6cff; border-radius:3px; }");
		sv->addWidget(opacitySlider, 0, Qt::AlignHCenter);
		opacityBadge = new QLabel(QString::number(OverlayOpacity()) + "%", sliderBox);
		opacityBadge->setAlignment(Qt::AlignHCenter); opacityBadge->setStyleSheet("color:#9a9ab8; background:transparent; font-size:10px;");
		sv->addWidget(opacityBadge);
		connect(opacitySlider, &QSlider::valueChanged, [this](int v) { SetOverlayOpacity(v); });
		sliderBox->hide();
		sliderBox->setObjectName("ovSliderBox");
		// simpan pointer sliderBox lewat properti dinamis toolbar untuk layout
		toolbar->setProperty("sliderBox", QVariant::fromValue((void*)sliderBox));

		LayoutChrome();
	}

	QWidget* SliderBox() const { return toolbar ? (QWidget*)toolbar->property("sliderBox").value<void*>() : nullptr; }

	void LayoutChrome()
	{
		if (!toolbar) return;
		int m = 8;
		if (dragHandle) { dragHandle->setGeometry(0, 0, width(), 16); dragHandle->raise(); }
		toolbar->adjustSize();
		toolbar->move(width() - toolbar->width() - m, 20); // kanan-atas (di bawah handle)
		if (auto sb = SliderBox())
		{
			sb->adjustSize();
			// Slider = jendela terpisah, didok tepat di LUAR tepi kanan overlay
			// (posisi global) sehingga tidak pernah menutupi toolbar/tombol X.
			QPoint tr = mapToGlobal(QPoint(width(), 0)); // titik kanan-atas overlay (global)
			int sy = tr.y() + (height() - sb->height()) / 2;
			sb->move(tr.x() + 6, sy);
		}
		if (clickThroughBadge)
		{
			clickThroughBadge->adjustSize();
			clickThroughBadge->move((width() - clickThroughBadge->width()) / 2, height() - clickThroughBadge->height() - 6);
			clickThroughBadge->raise();
		}
	}

	void ShowChrome(bool on)
	{
		if (minimized) on = false;
		if (toolbar) { toolbar->setVisible(on); toolbar->raise(); }
		if (dragHandle) { dragHandle->setVisible(on); dragHandle->raise(); }
		if (on) LayoutChrome(); // posisikan dulu baru tampilkan (jendela slider ikut overlay)
		if (auto sb = SliderBox()) { sb->setVisible(on); if (on) sb->raise(); }
	}

	void moveEvent(QMoveEvent* e) override
	{
		QDialog::moveEvent(e);
		// Ikutkan jendela slider saat overlay dipindah (bila sedang tampil).
		if (auto sb = SliderBox(); sb && sb->isVisible()) LayoutChrome();
	}

	void hideEvent(QHideEvent* e) override
	{
		QDialog::hideEvent(e);
		if (auto sb = SliderBox()) sb->hide(); // sembunyikan slider saat overlay disembunyikan
	}

	int OverlayOpacity() const
	{
		// simpan sebagai persen (10..100); default dari alpha background saat ini.
		return settings.value("Overlay opacity", 80).toInt();
	}

	void SetOverlayOpacity(int percent)
	{
		if (percent < 10) percent = 10; if (percent > 100) percent = 100;
		settings.setValue("Overlay opacity", percent);
		ApplyOverlayAlpha(percent * 255 / 100);
		if (opacityBadge) opacityBadge->setText(QString::number(percent) + "%");
		if (opacitySlider && opacitySlider->value() != percent) opacitySlider->setValue(percent);
	}

	void ToggleMinimize()
	{
		if (!minimized)
		{
			fullGeometry = geometry();
			minimized = true;
			ShowChrome(false);
			ui.display->hide();
			// pill kecil dengan logo/inisial
			setFixedSize(46, 46);
			update();
		}
		else
		{
			minimized = false;
			setMinimumSize(0, 0); setMaximumSize(16777215, 16777215);
			if (fullGeometry.isValid()) setGeometry(fullGeometry);
			ui.display->show();
			DisplaySentence(); // segarkan teks terbaru yang mungkin masuk saat minimized
			LayoutChrome();
			update();
		}
	}

	bool sizeLock, posLock, centeredText, autoResize, showOriginal, showOriginalAfterTranslation, useDictionary, clickThrough;
	bool furiganaOriginal = false, spaceWords = false; // Japanese Learning: overlay furigana + spasi antar kata
	bool showImmediately = true; // tampilkan overlay langsung di awal (dengan placeholder)
	QPoint oldPos;

	// --- Redesign: toolbar navigasi + slider transparansi + minimize/collapse ---
	QWidget* toolbar = nullptr;        // bar ikon mengambang (muncul saat hover)
	QSlider* opacitySlider = nullptr;  // slider transparansi vertikal (gaya volume)
	QLabel* opacityBadge = nullptr;    // label persen opacity di dekat slider
	QToolButton* pinBtn = nullptr;
	QToolButton* lockBtn = nullptr;
	QToolButton* clickBtn = nullptr;
	QWidget* dragHandle = nullptr;     // penanda area drag di header
	QLabel* clickThroughBadge = nullptr; // penanda "Click-through ON · Alt+X"
	bool minimized = false;            // sedang di-collapse ke pill?
	QRect fullGeometry;                // geometri sebelum collapse
	QString collapsedText;             // teks pill saat collapse
	// (BuildChrome/LayoutChrome/ShowChrome/SetOverlayOpacity/OverlayOpacity/ToggleMinimize
	//  didefinisikan sebagai member di bawah - tak perlu prototipe terpisah)

	class
	{
	public:
		bool CompareExchange(QLabel* display)
		{
			if (display->text() == text && display->font() == font && display->width() == width && display->alignment() == alignment) return false;
			text = display->text();
			font = display->font();
			width = display->width();
			alignment = display->alignment();
			return true;
		}

	private:
		QString text;
		QFont font;
		int width;
		Qt::Alignment alignment;
	} cachedDisplayInfo;
	std::vector<QPoint> textPositionMap;

	std::vector<QString> sentenceHistory;
	int historyIndex = 0;

	class DictionaryWindow : public PrettyWindow
	{
	public:
		DictionaryWindow() : PrettyWindow("Dictionary Window")
		{
			ui.display->setSizePolicy({ QSizePolicy::Fixed, QSizePolicy::Minimum });
		}

		void UpdateDictionary()
		{
			try
			{
				if (dictionaryFileLastWrite == std::filesystem::last_write_time(DICTIONARY_SAVE_FILE)) return;
				dictionaryFileLastWrite = std::filesystem::last_write_time(DICTIONARY_SAVE_FILE);
			}
			catch (std::filesystem::filesystem_error) { return; }

			dictionary.clear();
			charStorage.clear();

			auto StoreCopy = [&](std::string_view string)
			{
				auto location = &*charStorage.insert(charStorage.end(), string.begin(), string.end());
				charStorage.push_back(0);
				return location;
			};

			charStorage.reserve(std::filesystem::file_size(DICTIONARY_SAVE_FILE));
			std::ifstream stream(DICTIONARY_SAVE_FILE);
			BlockMarkupIterator savedDictionary(stream, Array<std::string_view>{ "|TERM|", "|DEFINITION|" });
			while (auto read = savedDictionary.Next())
			{
				const auto& [terms, definition] = read.value();
				auto storedDefinition = StoreCopy(definition);
				std::string_view termsView = terms;
				size_t start = 0, end = termsView.find("|TERM|");
				while (end != std::string::npos)
				{
					dictionary.push_back(DictionaryEntry{ StoreCopy(termsView.substr(start, end - start)), storedDefinition });
					start = end + 6;
					end = termsView.find("|TERM|", start);
				}
				dictionary.push_back(DictionaryEntry{ StoreCopy(termsView.substr(start)), storedDefinition });
			}
			std::stable_sort(dictionary.begin(), dictionary.end());

			inflections.clear();
			stream.seekg(0);
			BlockMarkupIterator savedInflections(stream, Array<std::string_view>{ "|ROOT|", "|INFLECTS TO|", "|NAME|" });
			while (auto read = savedInflections.Next())
			{
				const auto& [root, inflectsTo, name] = read.value();
				if (!inflections.emplace_back(Inflection{
					S(root),
					QRegularExpression(QRegularExpression::anchoredPattern(S(inflectsTo)), QRegularExpression::UseUnicodePropertiesOption),
					S(name)
				}).inflectsTo.isValid()) TEXTRACTOR_MESSAGE(L"Invalid regex: %s", StringToWideString(inflectsTo));
			}
		}

		void SetTerm(QString term)
		{
			this->term = term;
			UpdateDictionary();
			definitions.clear();
			definitionIndex = 0;
			std::unordered_set<const char*> foundDefinitions;
			for (term = term.left(100); !term.isEmpty(); term.chop(1))
				for (const auto& [rootTerm, definition, inflections] : LookupDefinitions(term, foundDefinitions))
					definitions.push_back(
						QStringLiteral("<h3>%1 (%5/%6)</h3><small>%2%3</small>%4").arg(
							term.split("<<")[0].toHtmlEscaped(),
							rootTerm.split("<<")[0].toHtmlEscaped(),
							inflections.join(""),
							definition
						)
					);
			for (int i = 0; i < definitions.size(); ++i) definitions[i] = definitions[i].arg(i + 1).arg(definitions.size());
			ShowDefinition();
		}

		void ShowDefinition()
		{
			if (definitions.empty()) return hide();
			ui.display->setText(definitions[definitionIndex]);
			adjustSize();
			resize(width(), 1);
			show();
		}

		struct DictionaryEntry
		{
			const char* term;
			const char* definition;
			bool operator<(DictionaryEntry other) const { return strcmp(term, other.term) < 0; }
		};
		std::vector<DictionaryEntry> dictionary;
		QString term;

	private:
		struct LookupResult
		{
			QString term;
			QString definition;
			QStringList inflectionsUsed;
		};
		std::vector<LookupResult> LookupDefinitions(QString term, std::unordered_set<const char*>& foundDefinitions, QStringList inflectionsUsed = {})
		{
			std::vector<LookupResult> results;
			for (auto [it, end] = std::equal_range(dictionary.begin(), dictionary.end(), DictionaryEntry{ term.toUtf8() }); it != end; ++it)
				if (foundDefinitions.emplace(it->definition).second) results.push_back({ term, it->definition, inflectionsUsed });
			for (const auto& inflection : inflections) if (auto match = inflection.inflectsTo.match(term); match.hasMatch())
			{
				QStringList currentInflectionsUsed = inflectionsUsed;
				currentInflectionsUsed.push_front(inflection.name);
				QString root;
				for (const auto& ch : inflection.root) root += ch.isDigit() ? match.captured(ch.digitValue()) : ch;
				for (const auto& definition : LookupDefinitions(root, foundDefinitions, currentInflectionsUsed)) results.push_back(definition);
			}
			return results;
		}

		void wheelEvent(QWheelEvent* event) override
		{
			int scroll = event->angleDelta().y();
			if (scroll > 0 && definitionIndex > 0) definitionIndex -= 1;
			if (scroll < 0 && definitionIndex + 1 < definitions.size()) definitionIndex += 1;
			int oldHeight = height();
			ShowDefinition();
			move(x(), y() + oldHeight - height());
		}

		struct Inflection
		{
			QString root;
			QRegularExpression inflectsTo;
			QString name;
		};
		std::vector<Inflection> inflections;

		std::filesystem::file_time_type dictionaryFileLastWrite;
		std::vector<char> charStorage;
		std::vector<QString> definitions;
		int definitionIndex;
	} dictionaryWindow;
} extraWindow;

bool ProcessSentence(std::wstring& sentence, SentenceInfo sentenceInfo)
{
	if (sentenceInfo["current select"] && sentenceInfo["text number"] != 0)
		QMetaObject::invokeMethod(&extraWindow, [sentence = S(sentence)] { extraWindow.AddSentence(sentence); });
	return false;
}
