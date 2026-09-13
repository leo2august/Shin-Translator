#include "extenwindow.h"
#include "ui_extenwindow.h"
#include <QMenu>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidgetItem>

extern const char* EXTENSIONS;
extern const char* ADD_EXTENSION;
extern const char* REMOVE_EXTENSION;
extern const char* INVALID_EXTENSION;
extern const char* CONFIRM_EXTENSION_OVERWRITE;
extern const char* EXTENSION_WRITE_ERROR;
extern const char* EXTEN_WINDOW_INSTRUCTIONS;

namespace
{
	constexpr auto EXTEN_SAVE_FILE = u8"SavedExtensions.txt";
	constexpr auto DEFAULT_EXTENSIONS = u8"Remove Repeated Characters>Regex Filter>VN Translate>Extra Window>Extra Newlines";

	struct Extension
	{
		std::wstring name;
		wchar_t* (*callback)(wchar_t*, const InfoForExtension*);
	};

	Ui::ExtenWindow ui;
	concurrency::reader_writer_lock extenMutex;
	std::vector<Extension> extensions;
	ExtenWindow* This = nullptr;

	bool Load(QString extenName)
	{
		if (extenName.endsWith(".dll")) extenName.chop(4);
		if (extenName.endsWith(".xdll")) extenName.chop(5);
		if (!QFile::exists(extenName + ".xdll")) QFile::copy(extenName + ".dll", extenName + ".xdll");
		// Extension must export "OnNewSentence"
		if (QTextFile(extenName + ".xdll", QIODevice::ReadOnly).readAll().contains("OnNewSentence"))
		{
			if (HMODULE module = LoadLibraryW(S(extenName + ".xdll").c_str()))
			{
				if (auto callback = (decltype(Extension::callback))GetProcAddress(module, "OnNewSentence"))
				{
					std::scoped_lock lock(extenMutex);
					extensions.push_back({ S(extenName), callback });
					return true;
				}
				FreeLibrary(module);
			}
		}
		return false;
	}

	void Unload(int index)
	{
		std::scoped_lock lock(extenMutex);
		FreeLibrary(GetModuleHandleW((extensions.at(index).name + L".xdll").c_str()));
		extensions.erase(extensions.begin() + index);
	}

	void Reorder(QStringList extenNames)
	{
		std::scoped_lock lock(extenMutex);
		std::vector<Extension> extensions;
		for (auto extenName : extenNames)
			extensions.push_back(*std::find_if(::extensions.begin(), ::extensions.end(), [&](Extension extension) { return extension.name == S(extenName); }));
		::extensions = extensions;
	}

	void DeleteByName(const QString& name); // fwd

	// Ikon huруф sederhana untuk tiap kategori ekstensi (identitas visual).
	QString ExtenGlyph(const QString& name)
	{
		QString n = name.toLower();
		if (n.contains("translate")) return QString::fromUtf8(u8"\U0001F4AC");   // terjemahan
		if (n.contains("window") || n.contains("overlay")) return QString::fromUtf8(u8"\U0001F5BC"); // overlay
		if (n.contains("regex") || n.contains("filter") || n.contains("replace")) return QString::fromUtf8(u8"\U0001F9F9"); // filter
		if (n.contains("repeat") || n.contains("newline") || n.contains("sentence")) return QString::fromUtf8(u8"\u2702"); // pembersih teks
		if (n.contains("clipboard") || n.contains("copy")) return QString::fromUtf8(u8"\U0001F4CB");
		if (n.contains("lua")) return QString::fromUtf8(u8"\U0001F4DC");
		return QString::fromUtf8(u8"\U0001F9E9");
	}

	// Bangun satu baris kartu ekstensi (nama + glyph + tombol hapus).
	QWidget* MakeExtenRow(const QString& name)
	{
		auto row = new QWidget();
		row->setObjectName("extenRow");
		auto h = new QHBoxLayout(row);
		h->setContentsMargins(12, 9, 10, 9);
		h->setSpacing(10);
		auto glyph = new QLabel(ExtenGlyph(name), row); glyph->setObjectName("extenGlyph");
		h->addWidget(glyph);
		auto nameLbl = new QLabel(name, row); nameLbl->setObjectName("extenName");
		h->addWidget(nameLbl, 1);
		auto grip = new QLabel(QString::fromUtf8(u8"\u2630"), row); grip->setObjectName("extenGrip");
		grip->setToolTip("Drag to reorder");
		h->addWidget(grip);
		auto del = new QPushButton(QString::fromUtf8(u8"\u2715"), row);
		del->setObjectName("extenDel");
		del->setCursor(Qt::PointingHandCursor);
		del->setToolTip("Remove this extension");
		del->setFixedSize(26, 26);
		QObject::connect(del, &QPushButton::clicked, [name] { DeleteByName(name); });
		h->addWidget(del);
		return row;
	}

	void Sync()
	{
		ui.extenList->clear();
		QTextFile extenSaveFile(EXTEN_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Truncate);
		concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
		for (auto extension : extensions)
		{
			QString name = S(extension.name);
			auto item = new QListWidgetItem(ui.extenList);
			item->setData(Qt::UserRole, name); // simpan nama utk reorder/drag
			item->setSizeHint(QSize(0, 46));
			ui.extenList->addItem(item);
			ui.extenList->setItemWidget(item, MakeExtenRow(name));
			extenSaveFile.write((name + ">").toUtf8());
		}
	}

	void Add(QFileInfo extenFile)
	{
		if (extenFile.suffix() == "dll" || extenFile.suffix() == "xdll")
		{
			if (extenFile.absolutePath() != QDir::currentPath())
			{
				if (QFile::exists(extenFile.fileName()) && QMessageBox::question(This, EXTENSIONS, CONFIRM_EXTENSION_OVERWRITE) == QMessageBox::Yes) QFile::remove(extenFile.fileName());
				if (!QFile::copy(extenFile.absoluteFilePath(), extenFile.fileName())) QMessageBox::warning(This, EXTENSIONS, EXTENSION_WRITE_ERROR);
			}
			if (Load(extenFile.fileName())) return Sync();
		}
		QMessageBox::information(This, EXTENSIONS, QString(INVALID_EXTENSION).arg(extenFile.fileName()));
	}

	void Delete()
	{
		if (ui.extenList->currentItem())
		{
			Unload(ui.extenList->currentIndex().row());
			Sync();
		}
	}

	void DeleteByName(const QString& name)
	{
		int index = -1;
		{
			concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
			std::wstring wname = S(name);
			for (int i = 0; i < (int)extensions.size(); ++i) if (extensions[i].name == wname) { index = i; break; }
		}
		if (index >= 0) { Unload(index); Sync(); }
	}

	void ContextMenu(QPoint point)
	{
		QAction addExtension(ADD_EXTENSION), removeExtension(REMOVE_EXTENSION);
		if (auto action = QMenu::exec({ &addExtension, &removeExtension }, ui.extenList->mapToGlobal(point), nullptr, This))
			if (action == &removeExtension) Delete();
			else if (QString extenFile = QFileDialog::getOpenFileName(This, ADD_EXTENSION, ".", EXTENSIONS + QString(" (*.xdll);;Libraries (*.dll)")); !extenFile.isEmpty()) Add(extenFile);
	}
}

bool IsExtensionLoaded(const QString& name)
{
	concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
	std::wstring wname = S(name);
	for (const auto& extension : extensions) if (extension.name == wname) return true;
	return false;
}

void SetExtensionEnabled(const QString& name, bool enabled)
{
	bool loaded = IsExtensionLoaded(name);
	if (enabled == loaded) return;
	if (enabled)
	{
		Load(name); // menambah ke akhir pipeline (setelah VN Translate) -> aman untuk overlay
	}
	else
	{
		int index = -1;
		{
			concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
			std::wstring wname = S(name);
			for (int i = 0; i < (int)extensions.size(); ++i) if (extensions[i].name == wname) { index = i; break; }
		}
		if (index >= 0) Unload(index);
	}
	// Perbarui daftar UI + file SavedExtensions.txt bila jendela ekstensi sudah dibuat.
	if (This) Sync();
	else
	{
		// Jendela ekstensi belum dibuat: tulis ulang file simpan langsung dari vektor.
		QTextFile extenSaveFile(EXTEN_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Truncate);
		concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
		for (const auto& extension : extensions) extenSaveFile.write((S(extension.name) + ">").toUtf8());
	}
}

bool DispatchSentenceToExtensions(std::wstring& sentence, const InfoForExtension* sentenceInfo)
{
	wchar_t* sentenceBuffer = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, (sentence.size() + 1) * sizeof(wchar_t));
	wcscpy_s(sentenceBuffer, sentence.size() + 1, sentence.c_str());
	concurrency::reader_writer_lock::scoped_lock_read readLock(extenMutex);
	for (const auto& extension : extensions)
		if (!*(sentenceBuffer = extension.callback(sentenceBuffer, sentenceInfo))) break;
	sentence = sentenceBuffer;
	HeapFree(GetProcessHeap(), 0, sentenceBuffer);
	return !sentence.empty();
}

void CleanupExtensions()
{
	std::scoped_lock lock(extenMutex);
	for (auto extension : extensions) FreeLibrary(GetModuleHandleW((extension.name + L".xdll").c_str()));
	extensions.clear();
}

ExtenWindow::ExtenWindow(QWidget* parent) : QMainWindow(parent, Qt::WindowCloseButtonHint)
{
	This = this;
	ui.setupUi(this);
	setWindowTitle(QString::fromUtf8(u8"Shin Translator \u2014 Extensions"));
	resize(460, 520);

	// Wadah utama diberi identitas (objectName) untuk styling dark-navy/violet.
	if (auto central = centralWidget()) central->setObjectName("extenRoot");

	// --- Header modern: judul + subjudul + tombol Add ---
	auto header = new QWidget(this);
	header->setObjectName("extenHeader");
	auto hl = new QHBoxLayout(header);
	hl->setContentsMargins(16, 12, 14, 12);
	auto titleBox = new QVBoxLayout(); titleBox->setSpacing(1);
	auto title = new QLabel(QString::fromUtf8(u8"\U0001F9E9  Extensions"), header);
	title->setObjectName("extenTitle");
	titleBox->addWidget(title);
	auto subtitle = new QLabel(QString::fromUtf8(u8"Drag to reorder \u2022 processed top to bottom"), header);
	subtitle->setObjectName("extenSubtitle");
	titleBox->addWidget(subtitle);
	hl->addLayout(titleBox);
	hl->addStretch();
	auto addBtn = new QPushButton(QString::fromUtf8(u8"+  Add"), header);
	addBtn->setObjectName("extenAdd");
	addBtn->setCursor(Qt::PointingHandCursor);
	connect(addBtn, &QPushButton::clicked, []
	{
		if (QString extenFile = QFileDialog::getOpenFileName(This, ADD_EXTENSION, ".", EXTENSIONS + QString(" (*.xdll);;Libraries (*.dll)")); !extenFile.isEmpty()) Add(extenFile);
	});
	hl->addWidget(addBtn);
	ui.vboxLayout->insertWidget(0, header);

	ui.extenList->setObjectName("extenList");
	ui.extenList->setSpacing(4);
	ui.extenList->setSelectionMode(QAbstractItemView::SingleSelection);

	// Footer hint.
	auto footer = new QLabel(QString::fromUtf8(
		u8"\u2715 removes an extension \u2022 drag \u2630 to change order \u2022 "
		u8"drop a .dll here to add"), this);
	footer->setObjectName("extenFooter");
	footer->setWordWrap(true);
	ui.vboxLayout->addWidget(footer);

	connect(ui.extenList, &QListWidget::customContextMenuRequested, ContextMenu);
	ui.extenList->installEventFilter(this);

	if (!QFile::exists(EXTEN_SAVE_FILE)) QTextFile(EXTEN_SAVE_FILE, QIODevice::WriteOnly).write(DEFAULT_EXTENSIONS);
	for (auto extenName : QString(QTextFile(EXTEN_SAVE_FILE, QIODevice::ReadOnly).readAll()).split(">")) Load(extenName);
	Sync();
}

bool ExtenWindow::eventFilter(QObject* target, QEvent* event)
{
	// https://stackoverflow.com/questions/1224432/how-do-i-respond-to-an-internal-drag-and-drop-operation-using-a-qlistwidget/1528215
	if (event->type() == QEvent::ChildRemoved)
	{
		QStringList extenNames;
		for (int i = 0; i < ui.extenList->count(); ++i) extenNames.push_back(ui.extenList->item(i)->data(Qt::UserRole).toString());
		Reorder(extenNames);
		Sync();
	}
	return false;
}

void ExtenWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Delete) Delete();
}

void ExtenWindow::dragEnterEvent(QDragEnterEvent* event)
{
	event->acceptProposedAction();
}

void ExtenWindow::dropEvent(QDropEvent* event)
{
	for (auto file : event->mimeData()->urls()) Add(file.toLocalFile());
}
