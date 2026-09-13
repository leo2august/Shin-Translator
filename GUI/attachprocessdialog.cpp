#include "attachprocessdialog.h"
#include <QtWinExtras/QtWin>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QListWidget>

extern const char* SELECT_PROCESS;
extern const char* ATTACH_INFO;

// Redesign: pemilih game modern (dark-navy/violet) - grid kartu ikon besar + nama,
// search bar, tombol jelas. Beda dari tampilan daftar polos Textractor.
AttachProcessDialog::AttachProcessDialog(QWidget* parent, std::vector<std::pair<QString, HICON>> processIcons) :
	QDialog(parent, Qt::WindowCloseButtonHint),
	model(this)
{
	ui.setupUi(this); // tetap panggil agar ui.* valid, lalu kita ganti isinya
	setObjectName("attachDialog");
	setWindowTitle(QString::fromUtf8(u8"Shin Translator \u2014 Select a game"));
	resize(760, 520);

	// Kosongkan layout .ui lama & bangun ulang.
	if (auto old = layout()) { QLayoutItem* it; while ((it = old->takeAt(0))) { if (it->widget()) it->widget()->deleteLater(); delete it; } delete old; }

	auto root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	// --- Header ---
	auto header = new QWidget(this); header->setObjectName("attachHeader");
	auto hl = new QVBoxLayout(header); hl->setContentsMargins(22, 16, 22, 14); hl->setSpacing(8);
	auto titleRow = new QHBoxLayout(); titleRow->setSpacing(10);
	auto logo = new QLabel(header);
	{
		QString logoPath = QCoreApplication::applicationDirPath() + "/logo_flower.png";
		QPixmap pm(logoPath);
		if (!pm.isNull()) logo->setPixmap(pm.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
	titleRow->addWidget(logo);
	auto title = new QLabel(QString::fromUtf8(u8"Choose a game to translate"), header);
	title->setObjectName("attachTitle");
	titleRow->addWidget(title); titleRow->addStretch();
	hl->addLayout(titleRow);
	auto sub = new QLabel(QString::fromUtf8(u8"Pick the running game window below, or type to search."), header);
	sub->setObjectName("attachSub");
	hl->addWidget(sub);
	// Search
	searchEdit = new QLineEdit(header);
	searchEdit->setObjectName("attachSearch");
	searchEdit->setPlaceholderText(QString::fromUtf8(u8"\U0001F50D  Search games / processes\u2026"));
	searchEdit->setClearButtonEnabled(true);
	hl->addWidget(searchEdit);
	root->addWidget(header);

	// --- Grid kartu game (QListWidget IconMode) ---
	list = new QListWidget(this);
	list->setObjectName("attachGrid");
	list->setViewMode(QListView::IconMode);
	list->setIconSize(QSize(48, 48));
	list->setResizeMode(QListView::Adjust);
	list->setMovement(QListView::Static);
	list->setSpacing(10);
	list->setUniformItemSizes(true);
	list->setWordWrap(true);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	root->addWidget(list, 1);

	QPixmap transparent(48, 48); transparent.fill(QColor::fromRgba(0));
	for (const auto& [process, icon] : processIcons)
	{
		auto item = new QListWidgetItem(icon ? QIcon(QtWin::fromHICON(icon)) : QIcon(transparent), process, list);
		item->setSizeHint(QSize(150, 92));
		item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
		item->setToolTip(process);
	}

	// --- Footer tombol ---
	auto footer = new QWidget(this); footer->setObjectName("attachFooter");
	auto fl = new QHBoxLayout(footer); fl->setContentsMargins(18, 12, 18, 14); fl->setSpacing(10);
	auto hint = new QLabel(QString::fromUtf8(u8"Tip: double-click a game to start instantly."), footer);
	hint->setObjectName("attachHint");
	fl->addWidget(hint); fl->addStretch();
	auto cancelBtn = new QPushButton(QString::fromUtf8(u8"Cancel"), footer);
	cancelBtn->setObjectName("attachCancel"); cancelBtn->setCursor(Qt::PointingHandCursor);
	fl->addWidget(cancelBtn);
	auto okBtn = new QPushButton(QString::fromUtf8(u8"\u2728  Start"), footer);
	okBtn->setObjectName("attachStart"); okBtn->setCursor(Qt::PointingHandCursor);
	okBtn->setDefault(true);
	fl->addWidget(okBtn);
	root->addWidget(footer);

	// --- Koneksi ---
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
	connect(list, &QListWidget::itemClicked, [this](QListWidgetItem* it) { selectedProcess = it->text(); });
	connect(list, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* it) { selectedProcess = it->text(); accept(); });
	connect(list, &QListWidget::currentItemChanged, [this](QListWidgetItem* it, QListWidgetItem*) { if (it) selectedProcess = it->text(); });
	connect(searchEdit, &QLineEdit::textChanged, [this](const QString& q)
	{
		for (int i = 0; i < list->count(); ++i)
			list->item(i)->setHidden(!list->item(i)->text().contains(q, Qt::CaseInsensitive));
	});
	connect(searchEdit, &QLineEdit::returnPressed, [this]
	{
		// Bila hanya satu yang cocok / ada yang terpilih -> mulai.
		if (!selectedProcess.isEmpty()) { accept(); return; }
		for (int i = 0; i < list->count(); ++i)
			if (!list->item(i)->isHidden()) { selectedProcess = list->item(i)->text(); accept(); return; }
	});
	searchEdit->setFocus();
}

QString AttachProcessDialog::SelectedProcess()
{
	return selectedProcess;
}
