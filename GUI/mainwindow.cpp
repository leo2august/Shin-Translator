#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "defs.h"
#include "module.h"
#include "extenwindow.h"
#include "../host/host.h"
#include "../host/hookcode.h"
#include "attachprocessdialog.h"
#include "lunabridge.h"
#include "japaneselearning.h"
#include "gameprofiles.h"
#include "ocrengine.h"
#include <QRubberBand>
#include <QPainter>
#include <QScreen>
#include <QMouseEvent>
#include <QKeyEvent>
#include <shellapi.h>
#include <QTimer>
#include <process.h>
#include <QRegularExpression>
#include <QStringListModel>
#include <QScrollBar>
#include <QMenu>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QHash>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPixmap>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QProcess>
#include <QStyle>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QDockWidget>
#include <QToolButton>
#include <QAction>
#include <QDialog>
#include <QListWidgetItem>
#include <QListView>
#include <filesystem>

extern const char* ATTACH;
extern const char* LAUNCH;
extern const char* CONFIG;
extern const char* DETACH;
extern const char* FORGET;
extern const char* ADD_HOOK;
extern const char* REMOVE_HOOKS;
extern const char* SAVE_HOOKS;
extern const char* SEARCH_FOR_HOOKS;
extern const char* SETTINGS;
extern const char* EXTENSIONS;
extern const char* FONT;
extern const char* SELECT_PROCESS;
extern const char* SELECT_PROCESS_INFO;
extern const char* FROM_COMPUTER;
extern const char* PROCESSES;
extern const char* CODE_INFODUMP;
extern const char* FAILED_TO_CREATE_CONFIG_FILE;
extern const char* HOOK_SEARCH_UNSTABLE_WARNING;
extern const char* HOOK_SEARCH_STARTING_VIEW_CONSOLE;
extern const char* SEARCH_CJK;
extern const char* SEARCH_PATTERN;
extern const char* SEARCH_DURATION;
extern const char* SEARCH_MODULE;
extern const char* PATTERN_OFFSET;
extern const char* MIN_ADDRESS;
extern const char* MAX_ADDRESS;
extern const char* STRING_OFFSET;
extern const char* MAX_HOOK_SEARCH_RECORDS;
extern const char* HOOK_SEARCH_FILTER;
extern const char* SEARCH_FOR_TEXT;
extern const char* TEXT;
extern const char* CODEPAGE;
extern const char* START_HOOK_SEARCH;
extern const char* SAVE_SEARCH_RESULTS;
extern const char* TEXT_FILES;
extern const char* DOUBLE_CLICK_TO_REMOVE_HOOK;
extern const char* SAVE_SETTINGS;
extern const char* USE_JP_LOCALE;
extern const char* FILTER_REPETITION;
extern const char* AUTO_ATTACH;
extern const char* ATTACH_SAVED_ONLY;
extern const char* SHOW_SYSTEM_PROCESSES;
extern const char* DEFAULT_CODEPAGE;
extern const char* FLUSH_DELAY;
extern const char* MAX_BUFFER_SIZE;
extern const char* MAX_HISTORY_SIZE;
extern const char* CONFIG_JP_LOCALE;
extern const wchar_t* ABOUT;
extern const wchar_t* CL_OPTIONS;
extern const wchar_t* LAUNCH_FAILED;
extern const wchar_t* INVALID_CODE;

namespace
{
	constexpr auto HOOK_SAVE_FILE = u8"SavedHooks.txt";
	constexpr auto GAME_SAVE_FILE = u8"SavedGames.txt";

	enum LaunchWithJapaneseLocale { PROMPT, ALWAYS, NEVER };

	Ui::MainWindow ui;
	std::atomic<DWORD> selectedProcessId = 0;
	ExtenWindow* extenWindow = nullptr;
	std::unordered_set<DWORD> alreadyAttached;
	bool autoAttach = false, autoAttachSavedOnly = true;
	bool showSystemProcesses = false;

	// --- VN Translator: Simple Mode (desain sesuai konsep UI/UX) ---
	constexpr auto SIMPLE_MODE = u8"Simple mode";
	QWidget* simplePanel = nullptr;    // panel mode simpel
	QWidget* advancedPanel = nullptr;  // pembungkus semua kontrol Textractor lama
	QLabel* simpleStatus = nullptr;    // label status kecil
	QLabel* headerStatus = nullptr;    // "● Ready / Connected" di header
	QLabel* engineBadge = nullptr;     // "Engine: Textractor / LunaHook"
	QPushButton* engineSwitchBtn = nullptr; // tombol "Switch to LunaHook/Textractor"
	QLabel* srcText = nullptr;         // kartu teks asli (Japanese)
	QLabel* dstText = nullptr;         // kartu terjemahan (bahasa target)
	QLabel* dstBadge = nullptr;        // badge bahasa target (di kartu tujuan) - diperbarui live
	// --- Japanese Learning (Fase 2, Opsi A) ---
	QString lastOriginal;              // teks asli terakhir (untuk render ulang saat toggle)
	QWidget* dictPanel = nullptr;      // side panel kamus (klik kata/kanji)
	QLabel* dictContent = nullptr;     // isi panel kamus
	QString dictCurrentWord;           // kata/kanji yang sedang ditampilkan
	QString dictCurrentReading, dictCurrentMeaning; int dictCurrentJlpt = 0;
	void ShowDictionary(const QString& segment);
	QString RenderOriginalWithLinks(const QString& original);
	void RefreshLearningUI();          // render ulang kartu Original sesuai setelan
	QLabel* gameNameLabel = nullptr;   // nama game terpilih
	QLabel* hookDot = nullptr;         // indikator "Hook connected"
	QWidget* historyList = nullptr;    // kontainer daftar history
	QVBoxLayout* historyLayout = nullptr;
	bool simpleMode = true;
	std::atomic<bool> autoPickThread = false; // sedang auto-detect sumber teks?
	int statTranslated = 0;
	// Auto Detect berbasis pola dialog: skor tiap thread; pilih yang paling mirip dialog VN.
	std::unordered_map<int64_t, int> threadDialogScore; // handle -> skor
	int64_t autoChosenHandle = 0;   // thread yang sedang dipilih auto
	int autoChosenScore = -1;
	std::atomic<unsigned long long> lastCurrentTextTick = 0; // kapan 'current' terakhir keluarkan teks (GetTickCount64)
	// --- Mesin hook: Textractor (bawaan) atau LunaHook (Opsi B, engine lebih banyak) ---
	std::atomic<int> sentencesSinceAttach = 0; // hitung teks masuk sejak attach (utk auto-fallback)
	std::atomic<bool> lunaEngineActive = false; // LunaHook sudah dijalankan utk sesi ini?
	DWORD lunaTargetPid = 0;                     // pid game utk LunaHook
	bool lunaStarted = false;                    // LunaBridge sudah di-Start?
	// --- Game Profiles ---
	QString currentGameExe;                      // nama exe game terpilih (untuk Save profile)
	QWidget* savedGamesList = nullptr;           // kontainer daftar game tersimpan (Simple Mode)
	QPushButton* saveGameBtn = nullptr;          // tombol "Save this game"
	// --- OCR (tangkap layar untuk game/menu yang tak bisa di-hook) ---
	QRect ocrRegion;                             // area layar OCR (global) tersimpan
	QTimer* ocrTimer = nullptr;                  // timer OCR berkala
	QString lastOcrText;                         // dedup: teks OCR terakhir
	QPushButton* ocrNowBtn = nullptr;
	QPushButton* ocrAutoBtn = nullptr;
	QLabel* ocrStatus = nullptr;
	void PickOcrRegion();                         // pilih area layar (drag kotak)
	void RunOcrOnce();                            // OCR sekali pada region tersimpan
	void ToggleAutoOcr();                         // hidupkan/matikan OCR berkala
	void HandleOcrText(const QString& text);      // salurkan teks OCR ke pipeline
	// Simpan hasil OCR (screenshot + teks + terjemahan) ke folder per-game.
	void HandleOcrResult(const QString& text, const QImage& shot);
	void SaveOcrSession(const QImage& shot, const QString& original, const QString& translation);
	QString OcrRootDir();                         // <appDir>/OCR
	QString OcrGameDir();                         // <appDir>/OCR/<game-or-General>
	// --- OCR History side panel (Task 3) ---
	QWidget* ocrHistoryPanel = nullptr;           // panel kanan berisi daftar sesi OCR
	QListWidget* ocrHistoryList = nullptr;        // daftar thumbnail
	QComboBox* ocrHistoryGame = nullptr;          // pilih folder game
	void ToggleOcrHistoryPanel();                 // buka/tutup panel
	void BuildOcrHistoryPanel();                  // bangun widget panel (sekali)
	void RefreshOcrHistoryGames();                // isi combo daftar game dari folder OCR/*
	void RefreshOcrHistoryList();                 // isi thumbnail sesuai game terpilih
	void RefreshOcrHistoryIfOpen();               // refresh bila panel sedang tampil
	void OpenOcrHistoryItem(const QString& jsonPath); // buka gambar+teks lagi
	void DeleteOcrHistoryItem(const QString& jsonPath);
	// --- Game Profiles ---
	void RefreshSavedGamesList();                // bangun ulang daftar game tersimpan
	void SaveCurrentGameProfile();               // simpan profil game aktif
	void ApplyGameProfileIfAny(const QString& exe, DWORD pid); // auto-terapkan saat pilih game
	void ImportGameProfiles();                   // impor profil (share) dari file
	void ExportGameProfiles();                   // ekspor profil ke file
	bool lunaX64Launched = false;                // (x86) sudah meluncurkan helper x64? (cegah tumpuk)
	// --- Text Processing (cache setelan, dibaca sekali; diperbarui saat Settings disimpan) ---
	std::atomic<bool> tpCleanEnabled = true;     // buang kode kontrol + runtuhkan pengulangan
	std::atomic<int> tpMinLen = 1;               // abaikan teks lebih pendek dari ini
	std::atomic<int> tpMaxLen = 2000;            // abaikan teks lebih panjang dari ini
	void RefreshTextProcessingSettings();        // muat ulang dari QSettings
	// --- Terjemahan ASINKRON (paralel) agar dialog cepat tidak saling menunggu ---
	std::atomic<uint64_t> translationSeq = 0;    // nomor urut kalimat (naik)
	std::atomic<uint64_t> lastShownSeq = 0;      // seq terakhir yang ditampilkan di kartu
	std::atomic<int> activeTranslations = 0;     // jumlah terjemahan berjalan (batasi konkuransi)
	void EnsureLunaStarted();                    // muat LunaHost + daftar output handler (sekali)
	void StartLunaEngine(DWORD processId, bool announce); // sambung+inject LunaHook
	void SwitchToTextractor(DWORD processId);    // kembali ke mesin Textractor bawaan
	bool LaunchX64ForLuna(DWORD processId);      // luncurkan x64 -luna<pid> dari x86
	void SetSimpleMode(bool on);
	void SimplePickGame();
	void SimpleAddPair(const QString& original, const QString& translation);
	void SimpleAddHistoryOnly(const QString& original, const QString& translation);
	void SimpleSetOriginal(const QString& original);
	void SetHeaderStatus(const QString& text, bool connected);
	void SetEngineBadge(bool luna);
	void RefreshTargetBadge();
	void Extensions();
	QString TargetLanguageBadge();
	void ResetSettingsToDefault();
	void OpenVocabulary();

	// Nilai kualitas SATU kalimat sebagai dialog VN (0..100, makin tinggi makin mirip dialog).
	// Fokus pada RASIO & panjang wajar, BUKAN jumlah karakter mentah -> teks sampah
	// yang sangat panjang tidak otomatis menang.
	int ScoreDialogText(const std::wstring& t)
	{
		int len = (int)t.size();
		if (len == 0) return 0;

		int cjk = 0, kanji = 0, kana = 0, digits = 0, latin = 0, punct = 0, control = 0, symbol = 0, bracket = 0;
		wchar_t prev = 0; int maxRepeat = 1, curRepeat = 1;
		std::unordered_set<wchar_t> uniqueLetters; // deteksi tabel kana (semua karakter unik)
		for (wchar_t c : t)
		{
			unsigned o = c;
			bool isKana = (0x3041 <= o && o <= 0x30ff);
			bool isKanji = (0x4e00 <= o && o <= 0x9fff);
			if (isKana || isKanji || (0xac00 <= o && o <= 0xd7a3)) { cjk++; uniqueLetters.insert(c); }
			if (isKana) kana++;
			if (isKanji) kanji++;
			if (o >= L'0' && o <= L'9') digits++;
			else if (o < 128 && iswalpha(c)) latin++;
			else if (o < 0x20) control++;
			else if (o >= 0x20 && o < 0x30 || (o >= 0x3a && o < 0x41)) symbol++;
			// kurung dialog jepang: penanda dialog VN paling kuat
			if (c == 0x300c || c == 0x300d || c == 0x300e || c == 0x300f) bracket++;
			if (c == 0x3002 || c == 0xff01 || c == 0xff1f || c == 0x3001 ||
				c == L'.' || c == L'!' || c == L'?' || c == L',' || c == L'\u2026') punct++;
			if (c == prev) { if (++curRepeat > maxRepeat) maxRepeat = curRepeat; }
			else curRepeat = 1;
			prev = c;
		}

		int letters = cjk + latin; // "isi" bermakna
		if (letters == 0) return 0; // hanya angka/simbol -> bukan dialog

		double cjkRatio = (double)cjk / len;    // proporsi karakter Jepang/CJK
		double symbolRatio = (double)(symbol + digits) / len; // proporsi ASCII simbol/angka

		// --- Tolak keras data mentah hook (mis. "!0.0,0(&F);ub&S)2 Stretch...") ---
		// Dialog VN nyata didominasi karakter CJK. Data sampah didominasi ASCII/simbol.
		if (cjk == 0) return 0;                 // tanpa CJK sama sekali -> bukan dialog Jepang
		if (symbolRatio > 0.35) return 0;       // terlalu banyak simbol/angka -> data mentah
		if (cjkRatio < 0.30) return 0;          // CJK terlalu sedikit -> bukan dialog

		// --- Tolak TABEL KANA / font atlas (mis. "…つやゆよあいうえおツヤユヨアイウエオ") ---
		// Ciri khas: hampir semua kana, TANPA kanji, dan rasio karakter UNIK sangat tinggi
		// (dialog nyata mengulang partikel/kana umum; tabel kana tiap huruf muncul sekali).
		if (cjk >= 8)
		{
			double uniqueRatio = (double)uniqueLetters.size() / cjk; // 1.0 = semua unik
			double kanaRatio = (double)kana / cjk;
			// Semua/hampir semua kana + hampir tak ada kanji + karakter nyaris semuanya unik
			// -> ini tabel kana/font, bukan dialog. Tolak.
			if (kanaRatio > 0.9 && kanji == 0 && uniqueRatio > 0.85) return 0;
		}
		// Didominasi kurung/tanda baca tanpa isi kalimat -> potongan sampah.
		int contentLetters = cjk - bracket; // huruf isi di luar kurung
		if (contentLetters <= 2 && (bracket + punct) >= 3) return 0;

		double score = 0;

		// 1) Rasio CJK = sinyal terkuat dialog Jepang.
		score += cjkRatio * 45;

		// 1b) Kanji = sinyal kuat kalimat nyata (dialog VN hampir selalu ada kanji).
		//     Tabel kana murni tidak dapat bonus ini.
		if (kanji > 0) score += 12;

		// 2) Panjang: dialog penuh (panjang wajar) menang atas potongan pendek.
		if (len >= 8 && len <= 120) score += 30;         // kalimat penuh
		else if (len >= 4 && len < 8) score += 16;       // frasa pendek
		else if (len < 4) score += 4;                    // sangat pendek (potongan)
		else if (len <= 240) score += 14;                // panjang, mungkin narasi
		else score += 2;                                 // dump raksasa

		// 3) Kurung dialog 「」 -> sinyal dialog VN paling kuat.
		if (bracket > 0) score += 18;
		// 4) Tanda baca kalimat Jepang -> ciri kalimat utuh.
		if (punct > 0) score += 10;

		// 5) Penalti tambahan: karakter kontrol, pengulangan mencurigakan, latin dominan.
		score -= control * 8;
		if (latin > cjk) score -= 20;           // lebih banyak latin daripada CJK -> curiga
		if (maxRepeat >= 6) score -= 30;

		if (score < 0) score = 0;
		if (score > 100) score = 100;
		return (int)score;
	}
	uint64_t savedThreadCtx = 0, savedThreadCtx2 = 0;
	wchar_t savedThreadCode[1000] = {};
	TextThread* current = nullptr;
	MainWindow* This = nullptr;

	void FindHooks();

	QString TextThreadString(TextThread& thread)
	{
		return QString("%1:%2:%3:%4:%5: %6").arg(
			QString::number(thread.handle, 16),
			QString::number(thread.tp.processId, 16),
			QString::number(thread.tp.addr, 16),
			QString::number(thread.tp.ctx, 16),
			QString::number(thread.tp.ctx2, 16)
		).toUpper().arg(S(thread.name));
	}

	ThreadParam ParseTextThreadString(QString ttString)
	{
		auto threadParam = ttString.splitRef(":");
		return { threadParam[1].toUInt(nullptr, 16), threadParam[2].toULongLong(nullptr, 16), threadParam[3].toULongLong(nullptr, 16), threadParam[4].toULongLong(nullptr, 16) };
	}

	std::array<InfoForExtension, 20> GetSentenceInfo(TextThread& thread)
	{
		void (*AddText)(int64_t, const wchar_t*) = [](int64_t number, const wchar_t* text)
		{
			QMetaObject::invokeMethod(This, [number, text = std::wstring(text)] { if (TextThread* thread = Host::GetThread(number)) thread->Push(text.c_str()); });
		};
		void (*AddSentence)(int64_t, const wchar_t*) = [](int64_t number, const wchar_t* sentence)
		{
			// pointer from Host::GetThread may not stay valid unless on main thread
			QMetaObject::invokeMethod(This, [number, sentence = std::wstring(sentence)] { if (TextThread* thread = Host::GetThread(number)) thread->AddSentence(sentence); });
		};
		DWORD (*GetSelectedProcessId)() = [] { return selectedProcessId.load(); };

		return
		{ {
		{ "current select", &thread == current },
		{ "text number", thread.handle },
		{ "process id", thread.tp.processId },
		{ "hook address", (int64_t)thread.tp.addr },
		{ "text handle", thread.handle },
		{ "text name", (int64_t)thread.name.c_str() },
		{ "add sentence", (int64_t)AddSentence },
		{ "add text", (int64_t)AddText },
		{ "get selected process id", (int64_t)GetSelectedProcessId },
		{ "void (*AddSentence)(int64_t number, const wchar_t* sentence)", (int64_t)AddSentence },
		{ "void (*AddText)(int64_t number, const wchar_t* text)", (int64_t)AddText },
		{ "DWORD (*GetSelectedProcessId)()", (int64_t)GetSelectedProcessId },
		{ nullptr, 0 } // nullptr marks end of info array
		} };
	}

	void AttachSavedProcesses()
	{
		std::unordered_set<std::wstring> attachTargets;
		if (autoAttach)
			for (auto process : QString(QTextFile(GAME_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", QString::SkipEmptyParts))
				attachTargets.insert(S(process));
		if (autoAttachSavedOnly)
			for (auto process : QString(QTextFile(HOOK_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", QString::SkipEmptyParts))
				attachTargets.insert(S(process.split(" , ")[0]));

		if (!attachTargets.empty())
			for (auto [processId, processName] : GetAllProcesses())
				if (processName && attachTargets.count(processName.value()) > 0 && alreadyAttached.count(processId) == 0) Host::InjectProcess(processId);
	}

	std::optional<std::wstring> UserSelectedProcess()
	{
		QStringList savedProcesses = QString::fromUtf8(QTextFile(GAME_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", QString::SkipEmptyParts);
		std::reverse(savedProcesses.begin(), savedProcesses.end());
		savedProcesses.removeDuplicates();
		savedProcesses.insert(1, FROM_COMPUTER);
		QString process = QInputDialog::getItem(This, SELECT_PROCESS, SELECT_PROCESS_INFO, savedProcesses, 0, true, &ok, Qt::WindowCloseButtonHint);
		if (process == FROM_COMPUTER) process = QDir::toNativeSeparators(QFileDialog::getOpenFileName(This, SELECT_PROCESS, "/", PROCESSES));
		if (ok && process.contains('\\')) return S(process);
		return {};
	}

	void ViewThread(int index)
	{
		ui.ttCombo->setCurrentIndex(index);
		ui.textOutput->setPlainText(sanitize(S((current = &Host::GetThread(ParseTextThreadString(ui.ttCombo->itemText(index))))->storage->c_str())));
		ui.textOutput->moveCursor(QTextCursor::End);
	}

	void AttachProcess()
	{
		QMultiHash<QString, DWORD> processesMap;
		std::vector<std::pair<QString, HICON>> processIcons;
		for (auto [processId, processName] : GetAllProcesses())
		{
			if (processName && (showSystemProcesses || processName->find(L":\\Windows\\") == std::string::npos))
			{
				QString fileName = QFileInfo(S(processName.value())).fileName();
				if (!processesMap.contains(fileName))
				{
					HICON bigIcon, smallIcon;
					ExtractIconExW(processName->c_str(), 0, &bigIcon, &smallIcon, 1);
					processIcons.push_back({ fileName, bigIcon ? bigIcon : smallIcon });
				}
				processesMap.insert(fileName, processId);
			}
		}
		std::sort(processIcons.begin(), processIcons.end(), [](auto one, auto two) { return QString::compare(one.first, two.first, Qt::CaseInsensitive) < 0; });

		AttachProcessDialog attachProcessDialog(This, processIcons);
		if (attachProcessDialog.exec())
		{
			QString process = attachProcessDialog.SelectedProcess();
			if (int processId = process.toInt(nullptr, 0)) Host::InjectProcess(processId);
			else for (int processId : processesMap.values(process)) Host::InjectProcess(processId);
		}
	}

	void LaunchProcess()
	{
		std::wstring process;
		if (auto selected = UserSelectedProcess()) process = selected.value();
		else return;
		std::wstring path = std::wstring(process).erase(process.rfind(L'\\'));

		PROCESS_INFORMATION info = {};
		auto useLocale = Settings().value(CONFIG_JP_LOCALE, PROMPT).toInt();
		if (!x64 && (useLocale == ALWAYS || (useLocale == PROMPT && QMessageBox::question(This, SELECT_PROCESS, USE_JP_LOCALE) == QMessageBox::Yes)))
		{
			if (HMODULE localeEmulator = LoadLibraryW(L"LoaderDll"))
			{
				// https://github.com/xupefei/Locale-Emulator/blob/aa99dec3b25708e676c90acf5fed9beaac319160/LEProc/LoaderWrapper.cs#L252
				struct
				{
					ULONG AnsiCodePage = SHIFT_JIS;
					ULONG OemCodePage = SHIFT_JIS;
					ULONG LocaleID = LANG_JAPANESE;
					ULONG DefaultCharset = SHIFTJIS_CHARSET;
					ULONG HookUiLanguageApi = FALSE;
					WCHAR DefaultFaceName[LF_FACESIZE] = {};
					TIME_ZONE_INFORMATION Timezone;
					ULONG64 Unused = 0;
				} LEB;
				GetTimeZoneInformation(&LEB.Timezone);
				((LONG(__stdcall*)(decltype(&LEB), LPCWSTR appName, LPWSTR commandLine, LPCWSTR currentDir, void*, void*, PROCESS_INFORMATION*, void*, void*, void*, void*))
					GetProcAddress(localeEmulator, "LeCreateProcess"))(&LEB, process.c_str(), NULL, path.c_str(), NULL, NULL, &info, NULL, NULL, NULL, NULL);
			}
		}
		if (info.hProcess == NULL)
		{
			STARTUPINFOW DUMMY = { sizeof(DUMMY) };
			CreateProcessW(process.c_str(), NULL, nullptr, nullptr, FALSE, 0, nullptr, path.c_str(), &DUMMY, &info);
		}
		if (info.hProcess == NULL) return Host::AddConsoleOutput(LAUNCH_FAILED);
		Host::InjectProcess(info.dwProcessId);
		CloseHandle(info.hProcess);
		CloseHandle(info.hThread);
	}

	void ConfigureProcess()
	{
		if (auto processName = GetModuleFilename(selectedProcessId)) if (int last = processName->rfind(L'\\') + 1)
		{
			std::wstring configFile = std::wstring(processName.value()).replace(last, std::string::npos, GAME_CONFIG_FILE);
			if (!std::filesystem::exists(configFile)) QTextFile(S(configFile), QFile::WriteOnly).write("see https://github.com/Artikash/Textractor/wiki/Game-configuration-file");
			if (std::filesystem::exists(configFile)) _wspawnlp(_P_DETACH, L"notepad", L"notepad", configFile.c_str(), NULL);
			else QMessageBox::critical(This, CONFIG, QString(FAILED_TO_CREATE_CONFIG_FILE).arg(S(configFile)));
		}
	}

	void DetachProcess()
	{
		try { Host::DetachProcess(selectedProcessId); }
		catch (std::out_of_range) {}
	}

	void ForgetProcess()
	{
		auto processName = GetModuleFilename(selectedProcessId);
		if (!processName) processName = UserSelectedProcess();
		DetachProcess();
		if (!processName) return;
		for (auto file : { GAME_SAVE_FILE, HOOK_SAVE_FILE })
		{
			QStringList lines = QString::fromUtf8(QTextFile(file, QIODevice::ReadOnly).readAll()).split("\n", QString::SkipEmptyParts);
			lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const QString& line) { return line.contains(S(processName.value())); }), lines.end());
			QTextFile(file, QIODevice::WriteOnly | QIODevice::Truncate).write(lines.join("\n").append("\n").toUtf8());
		}
	}

	void AddHook(QString hook)
	{
		if (QString hookCode = QInputDialog::getText(This, ADD_HOOK, CODE_INFODUMP, QLineEdit::Normal, hook, &ok, Qt::WindowCloseButtonHint); ok)
			if (hookCode.startsWith("S") || hookCode.startsWith("/S")) FindHooks(); // backwards compatibility for old hook search UX
			else if (auto hp = HookCode::Parse(S(hookCode))) try { Host::InsertHook(selectedProcessId, hp.value()); } catch (std::out_of_range) {}
			else Host::AddConsoleOutput(INVALID_CODE);
	}

	void AddHook()
	{
		AddHook("");
	}

	void RemoveHooks()
	{
		DWORD processId = selectedProcessId;
		std::unordered_map<uint64_t, HookParam> hooks;
		for (int i = 0; i < ui.ttCombo->count(); ++i)
		{
			ThreadParam tp = ParseTextThreadString(ui.ttCombo->itemText(i));
			if (tp.processId == selectedProcessId) hooks[tp.addr] = Host::GetThread(tp).hp;
		}
		auto hookList = new QListWidget(This);
		hookList->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
		hookList->setAttribute(Qt::WA_DeleteOnClose);
		hookList->setMinimumSize({ 300, 50 });
		hookList->setWindowTitle(DOUBLE_CLICK_TO_REMOVE_HOOK);
		for (auto [address, hp] : hooks) new QListWidgetItem(QString(hp.name) + "@" + QString::number(address, 16), hookList);
		QObject::connect(hookList, &QListWidget::itemDoubleClicked, [processId, hookList](QListWidgetItem* item)
		{
			try
			{
				Host::RemoveHook(processId, item->text().split("@")[1].toULongLong(nullptr, 16));
				delete item;
			}
			catch (std::out_of_range) { hookList->close(); }
		});
		hookList->show();
	}

	void SaveHooks()
	{
		auto processName = GetModuleFilename(selectedProcessId);
		if (!processName) return;
		QHash<uint64_t, QString> hookCodes;
		for (int i = 0; i < ui.ttCombo->count(); ++i)
		{
			ThreadParam tp = ParseTextThreadString(ui.ttCombo->itemText(i));
			if (tp.processId == selectedProcessId)
			{
				HookParam hp = Host::GetThread(tp).hp;
				if (!(hp.type & HOOK_ENGINE)) hookCodes[tp.addr] = S(HookCode::Generate(hp, tp.processId));
			}
		}
		auto hookInfo = QStringList() << S(processName.value()) << hookCodes.values();
		ThreadParam tp = current->tp;
		if (tp.processId == selectedProcessId) hookInfo << QString("|%1:%2:%3").arg(tp.ctx).arg(tp.ctx2).arg(S(HookCode::Generate(current->hp, tp.processId)));
		QTextFile(HOOK_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Append).write((hookInfo.join(" , ") + "\n").toUtf8());
	}

	void FindHooks()
	{
		QMessageBox::information(This, SEARCH_FOR_HOOKS, HOOK_SEARCH_UNSTABLE_WARNING);

		DWORD processId = selectedProcessId;
		SearchParam sp = {};
		sp.codepage = Host::defaultCodepage;
		bool searchForText = false, customSettings = false;
		QRegularExpression filter(".", QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::DotMatchesEverythingOption);

		QDialog dialog(This, Qt::WindowCloseButtonHint);
		QFormLayout layout(&dialog);
		QCheckBox asianCheck(&dialog);
		layout.addRow(SEARCH_CJK, &asianCheck);
		QDialogButtonBox confirm(QDialogButtonBox::Ok | QDialogButtonBox::Help | QDialogButtonBox::Retry, &dialog);
		layout.addRow(&confirm);
		confirm.button(QDialogButtonBox::Ok)->setText(START_HOOK_SEARCH);
		confirm.button(QDialogButtonBox::Retry)->setText(SEARCH_FOR_TEXT);
		confirm.button(QDialogButtonBox::Help)->setText(SETTINGS);
		QObject::connect(&confirm, &QDialogButtonBox::clicked, [&](QAbstractButton* button)
		{
			if (button == confirm.button(QDialogButtonBox::Retry)) searchForText = true;
			if (button == confirm.button(QDialogButtonBox::Help)) customSettings = true;
			dialog.accept();
		});
		dialog.setWindowTitle(SEARCH_FOR_HOOKS);
		if (!dialog.exec()) return;

		if (searchForText)
		{
			QDialog dialog(This, Qt::WindowCloseButtonHint);
			QFormLayout layout(&dialog);
			QLineEdit textEdit(&dialog);
			layout.addRow(TEXT, &textEdit);
			QSpinBox codepageSpin(&dialog);
			codepageSpin.setMaximum(INT_MAX);
			codepageSpin.setValue(sp.codepage);
			layout.addRow(CODEPAGE, &codepageSpin);
			QDialogButtonBox confirm(QDialogButtonBox::Ok);
			QObject::connect(&confirm, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
			layout.addRow(&confirm);
			if (!dialog.exec()) return;
			wcsncpy_s(sp.text, S(textEdit.text()).c_str(), PATTERN_SIZE - 1);
			try
			{
				Host::FindHooks(selectedProcessId, sp);
				ViewThread(0);
			} catch (std::out_of_range) {}
			return;
		}

		filter.setPattern(asianCheck.isChecked() ? "[\\x{3000}-\\x{a000}]{4,}" : "[\\x{0020}-\\x{1000}]{4,}");
		if (customSettings)
		{
			QDialog dialog(This, Qt::WindowCloseButtonHint);
			QFormLayout layout(&dialog);
			QLineEdit patternEdit(x64 ? "CC CC 48 89" : "55 8B EC", &dialog);
			assert(QByteArray::fromHex(patternEdit.text().toUtf8()) == QByteArray((const char*)sp.pattern, sp.length));
			layout.addRow(SEARCH_PATTERN, &patternEdit);
			for (auto [value, label] : Array<int&, const char*>{
				{ sp.searchTime, SEARCH_DURATION },
				{ sp.offset, PATTERN_OFFSET },
				{ sp.maxRecords, MAX_HOOK_SEARCH_RECORDS },
				{ sp.codepage, CODEPAGE },
			})
			{
				auto spinBox = new QSpinBox(&dialog);
				spinBox->setMaximum(INT_MAX);
				spinBox->setValue(value);
				layout.addRow(label, spinBox);
				QObject::connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), [&value](int newValue) { value = newValue; });
			}
			QLineEdit boundEdit(QFileInfo(S(GetModuleFilename(selectedProcessId).value_or(L""))).fileName(), &dialog);
			layout.addRow(SEARCH_MODULE, &boundEdit);
			for (auto [value, label] : Array<uintptr_t&, const char*>{
				{ sp.minAddress, MIN_ADDRESS },
				{ sp.maxAddress, MAX_ADDRESS },
				{ sp.padding, STRING_OFFSET },
			})
			{
				auto edit = new QLineEdit(QString::number(value, 16), &dialog);
				layout.addRow(label, edit);
				QObject::connect(edit, &QLineEdit::textEdited, [&value](QString text) { if (uintptr_t newValue = text.toULongLong(&ok, 16); ok) value = newValue; });
			}
			QLineEdit filterEdit(filter.pattern(), &dialog);
			layout.addRow(HOOK_SEARCH_FILTER, &filterEdit);
			QPushButton startButton(START_HOOK_SEARCH, &dialog);
			layout.addWidget(&startButton);
			QObject::connect(&startButton, &QPushButton::clicked, &dialog, &QDialog::accept);
			if (!dialog.exec()) return;
			if (patternEdit.text().contains('.'))
			{
				wcsncpy_s(sp.exportModule, S(patternEdit.text()).c_str(), MAX_MODULE_SIZE - 1);
				sp.length = 1;
			}
			else
			{
				QByteArray pattern = QByteArray::fromHex(patternEdit.text().replace("??", QString::number(XX, 16)).toUtf8());
				memcpy(sp.pattern, pattern.data(), sp.length = min(pattern.size(), PATTERN_SIZE));
			}
			wcsncpy_s(sp.boundaryModule, S(boundEdit.text()).c_str(), MAX_MODULE_SIZE - 1);
			filter.setPattern(filterEdit.text());
			if (!filter.isValid()) filter.setPattern(".");
		}
		else
		{
			sp.length = 0; // use default
		}
		filter.optimize();

		auto hooks = std::make_shared<QStringList>();
		try
		{
			Host::FindHooks(processId, sp,
				[hooks, filter](HookParam hp, std::wstring text) { if (filter.match(S(text)).hasMatch()) *hooks << sanitize(S(HookCode::Generate(hp) + L" => " + text)); });
		}
		catch (std::out_of_range) { return; }
		ViewThread(0);
		std::thread([hooks]
		{
			for (int lastSize = 0; hooks->size() == 0 || hooks->size() != lastSize; Sleep(2000)) lastSize = hooks->size();

			QString saveFileName;
			QMetaObject::invokeMethod(This, [&]
			{
				auto hookList = new QListView(This);
				hookList->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
				hookList->setAttribute(Qt::WA_DeleteOnClose);
				hookList->resize({ 750, 300 });
				hookList->setWindowTitle(SEARCH_FOR_HOOKS);
				if (hooks->size() > 5'000)
				{
					hookList->setUniformItemSizes(true); // they aren't actually uniform, but this improves performance
					hooks->push_back(QString(2000, '-')); // dumb hack: with uniform item sizes, the last item is assumed to be the largest
				}
				hookList->setModel(new QStringListModel(*hooks, hookList));
				QObject::connect(hookList, &QListView::clicked, [](QModelIndex i) { AddHook(i.data().toString().split(" => ")[0]); });
				hookList->show();

				saveFileName = QFileDialog::getSaveFileName(This, SAVE_SEARCH_RESULTS, "./results.txt", TEXT_FILES);
			}, Qt::BlockingQueuedConnection);
			if (!saveFileName.isEmpty())
			{
				QTextFile saveFile(saveFileName, QIODevice::WriteOnly | QIODevice::Truncate);
				for (auto hook = hooks->cbegin(); hook != hooks->cend(); ++hook) saveFile.write(hook->toUtf8().append('\n')); // QStringList::begin() makes a copy
			}
			hooks->clear();
		}).detach();
		QMessageBox::information(This, SEARCH_FOR_HOOKS, HOOK_SEARCH_STARTING_VIEW_CONSOLE);
	}

	// Reset semua pengaturan ke default, lalu restart aplikasi agar default termuat bersih.
	void ResetSettingsToDefault()
	{
		{
			Settings settings;
			// Hapus setting bahasa target (default -> English saat dibuat ulang).
			settings.remove("VN Translate");
			// Hapus setting teknis Textractor yang kita kelola.
			for (auto key : { FILTER_REPETITION, AUTO_ATTACH, ATTACH_SAVED_ONLY,
				SHOW_SYSTEM_PROCESSES, DEFAULT_CODEPAGE, FLUSH_DELAY, MAX_BUFFER_SIZE,
				MAX_HISTORY_SIZE, CONFIG_JP_LOCALE })
				settings.remove(key);
			settings.sync();
		}
		// Pastikan default bahasa target = English tertulis eksplisit.
		{
			Settings s; s.setValue("VN Translate/Translate to", "English"); s.sync();
		}
		// Restart aplikasi agar semua default (termasuk TextThread & Host) termuat ulang.
		QString exePath = QCoreApplication::applicationFilePath();
		QProcess::startDetached(exePath, {});
		CleanupExtensions();
		SetErrorMode(SEM_NOGPFAULTERRORBOX);
		ExitProcess(0);
	}

	// Jendela Vocabulary / Word Bank: daftar kata tersimpan, hapus per-kata.
	void OpenVocabulary()
	{
		auto& jl = JapaneseLearning::Instance();
		QDialog dialog(This, Qt::WindowCloseButtonHint);
		dialog.setObjectName("settingsDialog");
		dialog.setWindowTitle(QString::fromUtf8(u8"Shin Translator \u2014 Vocabulary"));
		dialog.resize(480, 560);
		auto outer = new QVBoxLayout(&dialog);
		outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);

		auto header = new QWidget(&dialog); header->setObjectName("extenHeader");
		auto hl = new QHBoxLayout(header); hl->setContentsMargins(16, 12, 14, 12);
		auto title = new QLabel(QString::fromUtf8(u8"\U0001F4D3  My Vocabulary"), header);
		title->setObjectName("extenTitle"); hl->addWidget(title); hl->addStretch();
		auto count = new QLabel(QString("%1 words").arg(jl.Vocabulary().size()), header);
		count->setObjectName("extenSubtitle"); hl->addWidget(count);
		outer->addWidget(header);

		auto list = new QListWidget(&dialog); list->setObjectName("extenList"); list->setSpacing(4);
		outer->addWidget(list, 1);

		std::function<void()> rebuild = [&]
		{
			list->clear();
			for (const auto& e : jl.Vocabulary())
			{
				auto item = new QListWidgetItem(list);
				item->setData(Qt::UserRole, e.word);
				item->setSizeHint(QSize(0, 58));
				auto row = new QWidget(); row->setObjectName("extenRow");
				auto rl = new QHBoxLayout(row); rl->setContentsMargins(12, 8, 10, 8); rl->setSpacing(10);
				QString jlpt = JapaneseLearning::JlptLabel(e.jlpt);
				auto info = new QLabel(QString("<span style='font-size:16px; color:#e7e7f5; font-weight:bold;'>%1</span>"
					"&nbsp;<span style='color:#9b8cff; font-size:12px;'>%2</span>%3<br>"
					"<span style='color:#8a8aa0; font-size:11px;'>%4</span>")
					.arg(e.word.toHtmlEscaped(), e.reading.toHtmlEscaped(),
						jlpt.isEmpty() ? QString() : QString("&nbsp;<span style='color:#22c55e; font-size:11px;'>%1</span>").arg(jlpt),
						e.meaning.toHtmlEscaped()), row);
				info->setTextFormat(Qt::RichText); info->setStyleSheet("background:transparent;");
				rl->addWidget(info, 1);
				auto del = new QPushButton(QString::fromUtf8(u8"\u2715"), row);
				del->setObjectName("extenDel"); del->setFixedSize(26, 26); del->setCursor(Qt::PointingHandCursor);
				QString w = e.word;
				QObject::connect(del, &QPushButton::clicked, [&jl, w, &rebuild, count] { jl.RemoveFromVocabulary(w); rebuild(); count->setText(QString("%1 words").arg(jl.Vocabulary().size())); });
				rl->addWidget(del);
				list->setItemWidget(item, row);
			}
			if (jl.Vocabulary().isEmpty())
			{
				auto item = new QListWidgetItem(list);
				item->setSizeHint(QSize(0, 60));
				auto empty = new QLabel(QString::fromUtf8(u8"No saved words yet. Click a word in the Original card, then \u2606 Save."));
				empty->setWordWrap(true); empty->setStyleSheet("color:#8a8aa0; padding:12px; background:transparent;");
				list->setItemWidget(item, empty);
			}
		};
		rebuild();
		dialog.exec();
	}

	void OpenSettings()
	{
		// ============ Panel Settings modern berkategori (VN Translator) ============
		// Sidebar kategori + halaman konten. Setting Textractor lama TIDAK dibuang,
		// dipindah ke kategori "Advanced". Tema dark-navy/violet.
		QDialog dialog(This, Qt::WindowCloseButtonHint);
		dialog.setObjectName("settingsDialog");
		dialog.setWindowTitle(QString::fromUtf8(u8"Shin Translator \u2014 Settings"));
		dialog.resize(720, 480);
		Settings settings(&dialog);

		auto outer = new QHBoxLayout(&dialog);
		outer->setContentsMargins(0, 0, 0, 0);
		outer->setSpacing(0);

		// --- Sidebar kategori ---
		auto nav = new QListWidget(&dialog);
		nav->setObjectName("settingsNav");
		nav->setFixedWidth(190);
		struct Cat { QString icon; QString name; };
		const std::vector<Cat> cats = {
			{ QString::fromUtf8(u8"\U0001F4AC"), "Translation" },
			{ QString::fromUtf8(u8"\U0001F9F9"), "Text Processing" },
			{ QString::fromUtf8(u8"\U0001F5BC"), "Display" },
			{ QString::fromUtf8(u8"\U0001F4D6"), "Japanese Learning" },
			{ QString::fromUtf8(u8"\u26A1"), "Performance" },
			{ QString::fromUtf8(u8"\U0001F3AF"), "Text Sources" },
			{ QString::fromUtf8(u8"\U0001F9E9"), "Extensions" },
			{ QString::fromUtf8(u8"\u2699"), "Advanced" },
		};
		for (const auto& c : cats) new QListWidgetItem(c.icon + "   " + c.name, nav);
		outer->addWidget(nav);

		auto pages = new QStackedWidget(&dialog);
		pages->setObjectName("settingsPages");
		outer->addWidget(pages, 1);

		auto makePage = [&](const QString& title) -> QVBoxLayout* {
			auto page = new QWidget(pages);
			auto scroll = new QScrollArea(pages);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			auto v = new QVBoxLayout(page);
			v->setContentsMargins(26, 22, 26, 22);
			v->setSpacing(14);
			auto h = new QLabel(title, page); h->setObjectName("setHead");
			v->addWidget(h);
			scroll->setWidget(page);
			pages->addWidget(scroll);
			return v;
		};

		// Kartu grup rapi.
		auto groupCard = [&](QVBoxLayout* parent, const QString& caption) -> QVBoxLayout* {
			if (!caption.isEmpty()) { auto s = new QLabel(caption); s->setObjectName("setSection"); parent->addWidget(s); }
			auto card = new QWidget(); card->setObjectName("setCard");
			auto v = new QVBoxLayout(card); v->setContentsMargins(16, 14, 16, 14); v->setSpacing(10);
			parent->addWidget(card);
			return v;
		};

		// ---------- 1) TRANSLATION ----------
		{
			auto v = makePage("Translation");
			auto g = groupCard(v, "");
			auto langLabel = new QLabel(QString::fromUtf8(u8"Translation language")); langLabel->setObjectName("setLabel");
			g->addWidget(langLabel);
			auto langCombo = new QComboBox();
			langCombo->setObjectName("setCombo");
			// Daftar bahasa target umum (VN Translate mendukung lebih banyak lewat Advanced).
			const QStringList langs = {
				"English", "Indonesian", "Spanish", "French", "German", "Portuguese",
				"Russian", "Chinese (Simplified)", "Chinese (Traditional)", "Korean",
				"Vietnamese", "Thai", "Italian", "Dutch", "Turkish", "Arabic",
				"Japanese",
			};
			langCombo->addItems(langs);
			QString curLang = settings.value("VN Translate/Translate to", "English").toString();
			int li = langCombo->findText(curLang);
			if (li < 0) { langCombo->insertItem(0, curLang); li = 0; }
			langCombo->setCurrentIndex(li);
			g->addWidget(langCombo);
			auto langHint = new QLabel(QString::fromUtf8(u8"Language of the translated text. Applies instantly, no restart needed."));
			langHint->setObjectName("setHint"); langHint->setWordWrap(true);
			g->addWidget(langHint);
			// Simpan + terapkan live.
			QObject::connect(langCombo, &QComboBox::currentTextChanged, [](const QString& lang)
			{
				Settings s; s.setValue("VN Translate/Translate to", lang);
				RefreshTargetBadge();
			});

			auto g2 = groupCard(v, "Behavior");
			auto engineNote = new QLabel(QString::fromUtf8(
				u8"Translation engine: VN Translate (free, automatic). Other providers "
				u8"(DeepL, Google, etc.) can be enabled via Extensions."));
			engineNote->setObjectName("setHint"); engineNote->setWordWrap(true);
			g2->addWidget(engineNote);
			v->addStretch();
		}

		// ---------- TEXT PROCESSING ----------
		{
			auto v = makePage("Text Processing");
			auto g = groupCard(v, "Cleaning");
			auto cleanChk = new QCheckBox(QString::fromUtf8(u8"Clean control codes & collapse repeated text"));
			cleanChk->setChecked(settings.value("Text Processing/Clean control codes", true).toBool());
			g->addWidget(cleanChk);
			QObject::connect(cleanChk, &QCheckBox::clicked, [](bool on)
			{ Settings().setValue("Text Processing/Clean control codes", on); RefreshTextProcessingSettings(); });
			auto cleanHint = new QLabel(QString::fromUtf8(
				u8"Removes engine control codes (e.g. %C, %N) and collapses text that "
				u8"the hook repeats (\"X X X\" \u2192 \"X\")."));
			cleanHint->setObjectName("setHint"); cleanHint->setWordWrap(true);
			g->addWidget(cleanHint);

			auto g2 = groupCard(v, "Length filter");
			auto addSpin = [&](const QString& label, const QString& key, int def, int maxV)
			{
				auto row = new QWidget(); auto rl = new QHBoxLayout(row); rl->setContentsMargins(0, 0, 0, 0);
				auto lbl = new QLabel(label); lbl->setObjectName("setLabel");
				auto spin = new QSpinBox(); spin->setRange(0, maxV);
				spin->setValue(settings.value("Text Processing/" + key, def).toInt());
				rl->addWidget(lbl, 1); rl->addWidget(spin);
				g2->addWidget(row);
				QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [key](int val)
				{ Settings().setValue("Text Processing/" + key, val); RefreshTextProcessingSettings(); });
			};
			addSpin(QString::fromUtf8(u8"Minimum text length"), "Min length", 1, 100);
			addSpin(QString::fromUtf8(u8"Maximum text length"), "Max length", 2000, 100000);
			auto lenHint = new QLabel(QString::fromUtf8(u8"Ignore captured text shorter or longer than these limits."));
			lenHint->setObjectName("setHint"); lenHint->setWordWrap(true);
			g2->addWidget(lenHint);

			auto g3 = groupCard(v, "Encoding");
			auto encRow = new QWidget(); auto erl2 = new QHBoxLayout(encRow); erl2->setContentsMargins(0, 0, 0, 0);
			auto encLbl = new QLabel(QString::fromUtf8(u8"Default codepage")); encLbl->setObjectName("setLabel");
			auto encCombo = new QComboBox();
			encCombo->addItem("Shift-JIS (932)", 932);
			encCombo->addItem("UTF-8 (65001)", 65001);
			encCombo->addItem("UTF-16 (1200)", 1200);
			encCombo->addItem("GBK (936)", 936);
			encCombo->addItem("Korean (949)", 949);
			int curCp = Host::defaultCodepage;
			int cpIdx = encCombo->findData(curCp); if (cpIdx < 0) { encCombo->addItem(QString("Codepage %1").arg(curCp), curCp); cpIdx = encCombo->count() - 1; }
			encCombo->setCurrentIndex(cpIdx);
			erl2->addWidget(encLbl, 1); erl2->addWidget(encCombo);
			g3->addWidget(encRow);
			QObject::connect(encCombo, qOverload<int>(&QComboBox::currentIndexChanged), [encCombo](int)
			{ int cp = encCombo->currentData().toInt(); Host::defaultCodepage = cp; Settings().setValue(DEFAULT_CODEPAGE, cp); });
			auto encHint = new QLabel(QString::fromUtf8(u8"Character encoding for games not using Unicode. Most JP games use Shift-JIS."));
			encHint->setObjectName("setHint"); encHint->setWordWrap(true);
			g3->addWidget(encHint);
			v->addStretch();
		}

		// ---------- DISPLAY (overlay controls) ----------
		{
			auto v = makePage("Display");
			auto g = groupCard(v, "Overlay");
			auto enable = new QCheckBox(QString::fromUtf8(u8"Enable floating translation overlay"));
			enable->setChecked(IsExtensionLoaded("Extra Window"));
			g->addWidget(enable);
			QObject::connect(enable, &QCheckBox::clicked, [](bool on) { SetExtensionEnabled("Extra Window", on); });

			// Kontrol overlay yang menulis QSettings group "Extra Window" (dibaca overlay).
			auto ow = [&](const QString& label, const QString& key, bool def)
			{
				auto chk = new QCheckBox(label);
				chk->setChecked(settings.value("Extra Window/" + key, def).toBool());
				g->addWidget(chk);
				QObject::connect(chk, &QCheckBox::clicked, [key](bool on) { Settings().setValue("Extra Window/" + key, on); });
			};
			ow(QString::fromUtf8(u8"Show original Japanese text in overlay"), "Original text", false);
			ow(QString::fromUtf8(u8"Show overlay immediately (with placeholder)"), "Show immediately", true);
			ow(QString::fromUtf8(u8"Furigana on original (JP) in overlay"), "Furigana original", false);
			ow(QString::fromUtf8(u8"Space between words (JP) in overlay"), "Space words", false);

			auto note = new QLabel(QString::fromUtf8(
				u8"These overlay options apply the next time the overlay appears. "
				u8"Font, colors, outline, and opacity can also be changed by right-clicking "
				u8"the overlay window directly."));
			note->setObjectName("setHint"); note->setWordWrap(true);
			g->addWidget(note);
			v->addStretch();
		}

		// ---------- JAPANESE LEARNING ----------
		{
			auto v = makePage("Japanese Learning");
			auto g = groupCard(v, "Reading assistance");
			auto dictChk = new QCheckBox(QString::fromUtf8(u8"Dictionary on click (tap a word/kanji in the Original card)"));
			dictChk->setChecked(settings.value("Japanese Learning/Dictionary on click", true).toBool());
			g->addWidget(dictChk);
			QObject::connect(dictChk, &QCheckBox::clicked, [](bool on)
			{ Settings().setValue("Japanese Learning/Dictionary on click", on); RefreshLearningUI(); });

			auto furiChk = new QCheckBox(QString::fromUtf8(u8"Show furigana (reading) for known words"));
			furiChk->setChecked(settings.value("Japanese Learning/Show furigana", false).toBool());
			g->addWidget(furiChk);
			QObject::connect(furiChk, &QCheckBox::clicked, [](bool on)
			{ Settings().setValue("Japanese Learning/Show furigana", on); RefreshLearningUI(); });

			auto note = new QLabel(QString::fromUtf8(
				u8"Offline dictionary: click a word or kanji in the Original card to see its "
				u8"reading, JLPT level, and meaning, then save it to your Vocabulary.\n\n"
				u8"Note: without a full sentence analyzer, furigana appears only for words "
				u8"found in the dictionary (full-sentence furigana is planned)."));
			note->setObjectName("setHint"); note->setWordWrap(true);
			g->addWidget(note);

			auto g2 = groupCard(v, "Vocabulary");
			auto vocabBtn = new QPushButton(QString::fromUtf8(u8"\U0001F4D3  Open Vocabulary / Word Bank"));
			QObject::connect(vocabBtn, &QPushButton::clicked, [&dialog] { dialog.accept(); OpenVocabulary(); });
			g2->addWidget(vocabBtn);
			v->addStretch();
		}

		// ---------- PERFORMANCE ----------
		{
			auto v = makePage("Performance");
			auto g = groupCard(v, "Responsiveness");
			auto addDelay = [&](const QString& label, const QString& key, int def, const QString& hint)
			{
				auto row = new QWidget(); auto rl = new QHBoxLayout(row); rl->setContentsMargins(0, 0, 0, 0);
				auto lbl = new QLabel(label); lbl->setObjectName("setLabel");
				auto spin = new QSpinBox(); spin->setRange(30, 2000); spin->setSuffix(" ms");
				spin->setValue(settings.value(key, def).toInt());
				rl->addWidget(lbl, 1); rl->addWidget(spin);
				g->addWidget(row);
				QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [key](int val) { Settings().setValue(key, val); });
				auto h = new QLabel(hint); h->setObjectName("setHint"); h->setWordWrap(true);
				g->addWidget(h);
			};
			addDelay(QString::fromUtf8(u8"LunaHook flush delay"), "Luna flush delay", 150,
				QString::fromUtf8(u8"Lower = text appears faster with LunaHook. Applies when LunaHook (re)starts."));
			// Textractor flush delay (langsung berlaku pada TextThread).
			{
				auto row = new QWidget(); auto rl = new QHBoxLayout(row); rl->setContentsMargins(0, 0, 0, 0);
				auto lbl = new QLabel(QString::fromUtf8(u8"Textractor flush delay")); lbl->setObjectName("setLabel");
				auto spin = new QSpinBox(); spin->setRange(30, 2000); spin->setSuffix(" ms");
				spin->setValue(TextThread::flushDelay);
				rl->addWidget(lbl, 1); rl->addWidget(spin);
				g->addWidget(row);
				QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [](int val)
				{ TextThread::flushDelay = val; Settings().setValue(FLUSH_DELAY, val); });
				auto h = new QLabel(QString::fromUtf8(u8"Lower = faster, but may split long lines. Default 500 ms."));
				h->setObjectName("setHint"); h->setWordWrap(true);
				g->addWidget(h);
			}

			auto g2 = groupCard(v, "Cache");
			auto cacheChk = new QCheckBox(QString::fromUtf8(u8"Cache translations (avoid re-translating identical lines)"));
			cacheChk->setChecked(settings.value("VN Translate/Use translation cache", true).toBool());
			g2->addWidget(cacheChk);
			QObject::connect(cacheChk, &QCheckBox::clicked, [](bool on) { Settings().setValue("VN Translate/Use translation cache", on); });
			auto cacheHint = new QLabel(QString::fromUtf8(u8"Speeds up repeated lines and reduces network requests."));
			cacheHint->setObjectName("setHint"); cacheHint->setWordWrap(true);
			g2->addWidget(cacheHint);
			v->addStretch();
		}

		// ---------- TEXT SOURCES (ramah pengguna) ----------
		{
			auto v = makePage("Text Sources");
			auto g = groupCard(v, "");
			auto note = new QLabel(QString::fromUtf8(
				u8"Shin Translator picks the dialogue text source automatically (Auto Detect).\n\n"
				u8"If a game is not detected, use the \"Try LunaHook\" button on the main "
				u8"screen (an alternative engine for unreadable game engines).\n\n"
				u8"Technical details (hook, thread, process) are available in Advanced mode."));
			note->setObjectName("setHint"); note->setWordWrap(true);
			g->addWidget(note);
			v->addStretch();
		}

		// ---------- 5) EXTENSIONS ----------
		{
			auto v = makePage("Extensions");
			auto g = groupCard(v, "");
			auto note = new QLabel(QString::fromUtf8(
				u8"Extensions add features (alternative translators, text filters, overlay, "
				u8"etc.). Manage the order and enable/disable extensions in the manager window."));
			note->setObjectName("setHint"); note->setWordWrap(true);
			g->addWidget(note);
			auto openExt = new QPushButton(QString::fromUtf8(u8"\U0001F9E9  Open Extensions Manager"));
			QObject::connect(openExt, &QPushButton::clicked, [&dialog] { dialog.accept(); Extensions(); });
			g->addWidget(openExt);
			v->addStretch();
		}

		// ---------- 6) ADVANCED (setting Textractor lama, dipindah ke sini) ----------
		QPushButton* saveButton = new QPushButton(SAVE_SETTINGS);
		{
			auto v = makePage("Advanced");

			auto gp = groupCard(v, "Process");
			for (auto [value, label] : Array<bool&, const char*>{
				{ autoAttach, AUTO_ATTACH },
				{ autoAttachSavedOnly, ATTACH_SAVED_ONLY },
				{ showSystemProcesses, SHOW_SYSTEM_PROCESSES },
				{ TextThread::filterRepetition, FILTER_REPETITION },
			})
			{
				auto checkBox = new QCheckBox(label);
				checkBox->setChecked(value);
				gp->addWidget(checkBox);
				QObject::connect(saveButton, &QPushButton::clicked, [checkBox, label, &value] { Settings().setValue(label, value = checkBox->isChecked()); });
			}

			auto gt = groupCard(v, "Text extraction");
			for (auto [value, label] : Array<int&, const char*>{
				{ TextThread::maxBufferSize, MAX_BUFFER_SIZE },
				{ TextThread::flushDelay, FLUSH_DELAY },
				{ TextThread::maxHistorySize, MAX_HISTORY_SIZE },
				{ Host::defaultCodepage, DEFAULT_CODEPAGE },
			})
			{
				auto row = new QWidget(); auto rl = new QHBoxLayout(row); rl->setContentsMargins(0, 0, 0, 0);
				auto lbl = new QLabel(label); lbl->setObjectName("setLabel");
				auto spinBox = new QSpinBox();
				spinBox->setMaximum(INT_MAX);
				spinBox->setValue(value);
				rl->addWidget(lbl, 1); rl->addWidget(spinBox);
				gt->addWidget(row);
				QObject::connect(saveButton, &QPushButton::clicked, [spinBox, label, &value] { Settings().setValue(label, value = spinBox->value()); });
			}

			auto gl = groupCard(v, "Locale");
			auto localeRow = new QWidget(); auto llr = new QHBoxLayout(localeRow); llr->setContentsMargins(0, 0, 0, 0);
			auto localeLbl = new QLabel(CONFIG_JP_LOCALE); localeLbl->setObjectName("setLabel");
			auto localeCombo = new QComboBox();
			assert(PROMPT == 0 && ALWAYS == 1 && NEVER == 2);
			localeCombo->addItems({ { "Prompt", "Always", "Never" } });
			localeCombo->setCurrentIndex(settings.value(CONFIG_JP_LOCALE, PROMPT).toInt());
			llr->addWidget(localeLbl, 1); llr->addWidget(localeCombo);
			gl->addWidget(localeRow);
			QObject::connect(localeCombo, qOverload<int>(&QComboBox::activated), [](int i) { Settings().setValue(CONFIG_JP_LOCALE, i); });

			// Baris tombol: Save + Reset to default.
			auto btnRow = new QWidget(); auto brl = new QHBoxLayout(btnRow); brl->setContentsMargins(0, 6, 0, 0);
			auto resetButton = new QPushButton(QString::fromUtf8(u8"Reset to default"));
			resetButton->setObjectName("setGhost");
			brl->addWidget(resetButton);
			brl->addStretch();
			brl->addWidget(saveButton);
			v->addWidget(btnRow);
			v->addStretch();

			// Reset semua pengaturan ke default (bahasa target -> English, setting teknis -> default).
			QObject::connect(resetButton, &QPushButton::clicked, [&dialog]
			{
				if (QMessageBox::question(&dialog, QString::fromUtf8(u8"Reset to default"),
					QString::fromUtf8(u8"Reset all settings to their default values? "
						u8"The app will restart to apply the changes.")) != QMessageBox::Yes) return;
				ResetSettingsToDefault();
			});
		}

		// Footer save (berlaku untuk halaman Advanced) + tutup.
		QObject::connect(saveButton, &QPushButton::clicked, &dialog, &QDialog::accept);

		QObject::connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
		nav->setCurrentRow(0);
		dialog.exec();
	}

	void Extensions()
	{
		extenWindow->activateWindow();
		extenWindow->showNormal();
	}

	void SetOutputFont(QString fontString)
	{
		QFont font = ui.textOutput->font();
		font.fromString(fontString);
		font.setStyleStrategy(QFont::NoFontMerging);
		ui.textOutput->setFont(font);
		Settings().setValue(FONT, font.toString());
	}

	void ProcessConnected(DWORD processId)
	{
		alreadyAttached.insert(processId);

		QString process = S(GetModuleFilename(processId).value_or(L"???"));
		QMetaObject::invokeMethod(This, [process, processId]
		{
			ui.processCombo->addItem(QString::number(processId, 16).toUpper() + ": " + QFileInfo(process).fileName());
		});
		if (process == "???") return;

		// This does add (potentially tons of) duplicates to the file, but as long as I don't perform Ω(N^2) operations it shouldn't be an issue
		QTextFile(GAME_SAVE_FILE, QIODevice::WriteOnly | QIODevice::Append).write((process + "\n").toUtf8());

		QStringList allProcesses = QString(QTextFile(HOOK_SAVE_FILE, QIODevice::ReadOnly).readAll()).split("\n", QString::SkipEmptyParts);
		auto hookList = std::find_if(allProcesses.rbegin(), allProcesses.rend(), [&](QString hookList) { return hookList.contains(process); });
		bool hasSavedHook = hookList != allProcesses.rend();
		if (hasSavedHook)
			for (auto hookInfo : hookList->split(" , "))
				if (auto hp = HookCode::Parse(S(hookInfo))) Host::InsertHook(processId, hp.value());
				else swscanf_s(S(hookInfo).c_str(), L"|%I64d:%I64d:%[^\n]", &savedThreadCtx, &savedThreadCtx2, savedThreadCode, (unsigned)std::size(savedThreadCode));

		// Game Profile (termasuk hasil import/share): bila exe game cocok & punya hookCode
		// Textractor, pasang hook itu langsung (agar teks muncul tanpa deteksi ulang,
		// termasuk untuk profil yang di-share dari pengguna lain di path berbeda).
		{
			QString exe = QFileInfo(process).fileName();
			if (auto prof = GameProfiles::Instance().Find(exe); prof && prof->engine == "textractor" && !prof->hookCode.isEmpty())
				if (auto hp = HookCode::Parse(S(prof->hookCode))) { Host::InsertHook(processId, hp.value()); hasSavedHook = true; }
		}

		// VN Translator Auto Detect: HANYA mengandalkan hook engine bawaan yang AMAN.
		// Kita TIDAK menjalankan hook-search otomatis karena pemindaian memori bisa
		// membuat sebagian game crash (fitur "Search for hooks" tetap ada di Advanced,
		// lengkap dengan peringatannya). Bila engine bawaan tak memberi teks dalam
		// beberapa detik, arahkan pengguna ke Mode Advanced.
		if (simpleMode && autoPickThread && !hasSavedHook)
		{
			std::thread([]
			{
				Sleep(8000);
				if (!autoPickThread) return; // sudah dapat thread dari engine bawaan
				QMetaObject::invokeMethod(This, []
				{
					if (!autoPickThread || !simpleStatus) return;
					simpleStatus->setText(QString::fromUtf8(
						u8"No text detected yet. Open a dialogue in the game; if it stays empty, "
						u8"try \u2699 Advanced \u2192 Search for hooks."));
				});
			}).detach();
		}
	}

	void ProcessDisconnected(DWORD processId)
	{
		QMetaObject::invokeMethod(This, [processId]
		{
			ui.processCombo->removeItem(ui.processCombo->findText(QString::number(processId, 16).toUpper() + ":", Qt::MatchStartsWith));
		}, Qt::BlockingQueuedConnection);
	}

	void ThreadAdded(TextThread& thread)
	{
		std::wstring threadCode = HookCode::Generate(thread.hp, thread.tp.processId);
		bool savedMatch = (savedThreadCtx & 0xFFFF) == (thread.tp.ctx & 0xFFFF) && savedThreadCtx2 == thread.tp.ctx2 && savedThreadCode == threadCode;
		if (savedMatch)
		{
			savedThreadCtx = savedThreadCtx2 = savedThreadCode[0] = 0;
			current = &thread;
		}
		QMetaObject::invokeMethod(This, [savedMatch, ttString = TextThreadString(thread) + S(FormatString(L" (%s)", threadCode))]
		{
			ui.ttCombo->addItem(ttString);
			if (savedMatch) ViewThread(ui.ttCombo->count() - 1);
			// Mode simpel: TIDAK langsung memilih thread pertama. Biarkan skoring
			// dialog di SentenceReceived yang memilih thread paling mirip dialog VN.
		});
	}

	void ThreadRemoved(TextThread& thread)
	{
		QMetaObject::invokeMethod(This, [ttString = TextThreadString(thread)]
		{
			int threadIndex = ui.ttCombo->findText(ttString, Qt::MatchStartsWith);
			if (threadIndex == ui.ttCombo->currentIndex())	ViewThread(0);
			ui.ttCombo->removeItem(threadIndex);
		}, Qt::BlockingQueuedConnection);
	}

	// Muat ulang setelan Text Processing dari QSettings (dipanggil saat start & saat disimpan).
	void RefreshTextProcessingSettings()
	{
		Settings s;
		tpCleanEnabled = s.value("Text Processing/Clean control codes", true).toBool();
		tpMinLen = s.value("Text Processing/Min length", 1).toInt();
		tpMaxLen = s.value("Text Processing/Max length", 2000).toInt();
	}

	// Bersihkan teks hook: buang kode kontrol (mis. "%C", "%N") & runtuhkan pengulangan
	// frasa berturut-turut ("X X X" -> "X"). Dipakai untuk teks LunaHook & Textractor.
	void CleanHookText(std::wstring& s)
	{
		// 1) Buang kode kontrol gaya engine: '%' diikuti satu huruf ASCII (mis. %C, %N, %P, %K).
		std::wstring out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == L'%' && i + 1 < s.size() && ((s[i + 1] >= L'A' && s[i + 1] <= L'Z') || (s[i + 1] >= L'a' && s[i + 1] <= L'z')))
			{
				i++; // lewati '%' dan huruf kode
				continue;
			}
			// Kode escape gaya engine dengan backslash:
			//   \#0x3010;  \#RRGGBB;  (warna/ruby)  -> buang seluruhnya
			//   \n \@ \k \e \p \| \> \< \. \_  (jeda/klik/newline literal) -> buang
			if (s[i] == L'\\' && i + 1 < s.size())
			{
				wchar_t nx = s[i + 1];
				// \#....;  -> lewati sampai ';' (atau sampai batas wajar).
				if (nx == L'#')
				{
					size_t j = i + 2;
					size_t limit = (s.size() < i + 16) ? s.size() : (i + 16); // batasi agar tak makan teks
					while (j < s.size() && j < limit && s[j] != L';') j++;
					if (j < s.size() && s[j] == L';') { i = j; continue; } // lewati '#...;'
				}
				// \<huruf tunggal> escape umum -> buang keduanya.
				if (nx == L'n' || nx == L'@' || nx == L'k' || nx == L'e' || nx == L'p' ||
					nx == L'|' || nx == L'>' || nx == L'<' || nx == L'.' || nx == L'_' || nx == L'r')
				{
					i++; // lewati '\' dan huruf escape
					continue;
				}
			}
			// Buang karakter kontrol tak terlihat (kecuali newline & zero-width separator kita).
			if (s[i] < 0x20 && s[i] != L'\n' && s[i] != 0x200b) continue;
			out.push_back(s[i]);
		}
		s = std::move(out);

		// Buang tag nama/ruby kurung sudut 【...】 di AWAL (label pembicara engine).
		// Contoh hasil: "【Sofia】" -> hilangkan agar tak ikut diterjemahkan jadi noise.
		{
			size_t p = 0;
			while (p < s.size() && (s[p] == L' ' || s[p] == L'\u3000')) p++;
			if (p < s.size() && s[p] == 0x3010) // 【
			{
				size_t close = s.find(0x3011, p); // 】
				if (close != std::wstring::npos && close - p <= 24)
				{
					s.erase(0, close + 1);
					while (!s.empty() && (s.front() == L' ' || s.front() == L'\u3000' || s.front() == L'\n')) s.erase(0, 1);
				}
			}
		}

		// 2) Runtuhkan pengulangan frasa berturut-turut yang identik.
		//    Cari unit terpendek yang, bila diulang, menyusun seluruh string (setelah trim).
		auto trimmed = s;
		while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\u3000' || trimmed.back() == L'\n')) trimmed.pop_back();
		size_t start = 0;
		while (start < trimmed.size() && (trimmed[start] == L' ' || trimmed[start] == L'\u3000')) start++;
		trimmed = trimmed.substr(start);
		if (trimmed.size() >= 4)
		{
			int n = (int)trimmed.size();
			for (int unit = 2; unit <= n / 2; ++unit)
			{
				if (n % unit != 0) continue;
				std::wstring first = trimmed.substr(0, unit);
				bool allSame = true;
				for (int p = unit; p < n; p += unit)
					if (trimmed.compare(p, unit, first) != 0) { allSame = false; break; }
				if (allSame) { s = first; return; }
			}
			// Pengulangan dengan pemisah spasi/新行: "X X X" atau "X\nX".
			for (wchar_t sep : { L' ', L'\u3000', L'\n' })
			{
				size_t sp = trimmed.find(sep);
				if (sp != std::wstring::npos && sp >= 2)
				{
					std::wstring unit = trimmed.substr(0, sp);
					std::wstring rest = trimmed;
					bool allSame = true; int reps = 0;
					size_t pos = 0;
					while (pos < rest.size())
					{
						if (rest.compare(pos, unit.size(), unit) != 0) { allSame = false; break; }
						pos += unit.size();
						reps++;
						while (pos < rest.size() && (rest[pos] == sep)) pos++;
					}
					if (allSame && reps >= 2) { s = unit; return; }
				}
			}
		}
	}

	// Pemilih area OCR: overlay fullscreen semi-transparan; user drag kotak.
	// Callback dipanggil dengan QRect (koordinat global) saat selesai; batal -> rect kosong.
	class OcrRegionPicker : public QWidget
	{
	public:
		OcrRegionPicker(std::function<void(QRect)> onDone)
			: onDone(onDone), rubber(QRubberBand::Rectangle, this)
		{
			setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
			// WA_TranslucentBackground WAJIB agar lapisan benar-benar tembus pandang
			// (tanpa ini, latar default abu-abu solid menutupi game).
			setAttribute(Qt::WA_TranslucentBackground);
			setAttribute(Qt::WA_DeleteOnClose);
			setCursor(Qt::CrossCursor);
			setMouseTracking(true);
			// Bentang seluruh virtual desktop (semua monitor).
			QRect vg;
			for (QScreen* scr : QApplication::screens()) vg = vg.united(scr->geometry());
			setGeometry(vg);
			virtualOrigin = vg.topLeft();
		}
	protected:
		// Hitung posisi tombol Capture & Cancel (di bawah kotak seleksi).
		void computeButtons()
		{
			if (sel.isNull() || !hasSelection) { captureBtn = cancelBtn = QRect(); return; }
			int bw = 108, bh = 34, gap = 8;
			int by = sel.bottom() + 10;
			if (by + bh > height() - 6) by = sel.top() - bh - 10;      // taruh di atas bila mepet bawah
			if (by < 6) by = qMin(sel.bottom() + 10, height() - bh - 6);
			int totalW = bw * 2 + gap;
			int bx = sel.center().x() - totalW / 2;
			if (bx < 6) bx = 6; if (bx + totalW > width() - 6) bx = width() - 6 - totalW;
			captureBtn = QRect(bx, by, bw, bh);
			cancelBtn = QRect(bx + bw + gap, by, bw, bh);
		}
		void paintEvent(QPaintEvent*) override
		{
			QPainter p(this);
			p.setRenderHint(QPainter::Antialiasing);
			QColor dim(10, 12, 24, 40); // redup tipis -> game tetap terlihat
			if (!hasSelection || sel.isNull()) p.fillRect(rect(), dim);
			else
			{
				QRegion outside = QRegion(rect()).subtracted(QRegion(sel));
				p.setClipRegion(outside);
				p.fillRect(rect(), dim);
				p.setClipping(false);
				p.setPen(QPen(QColor(124, 108, 255), 2));
				p.drawRect(sel);
				p.setPen(QColor(255, 255, 255));
				p.drawText(sel.adjusted(2, -20, 0, 0), Qt::AlignLeft | Qt::AlignTop,
					QString("%1 x %2").arg(sel.width()).arg(sel.height()));
			}
			// Banner instruksi.
			QString hint = hasSelection
				? QString::fromUtf8(u8"Click Capture to OCR this area, or Cancel")
				: QString::fromUtf8(u8"Drag to select the OCR area");
			QFont f = p.font(); f.setPointSize(12); f.setBold(true); p.setFont(f);
			QRect tr = p.fontMetrics().boundingRect(hint).adjusted(-16, -8, 16, 8);
			tr.moveCenter(QPoint(rect().center().x(), rect().top() + 44));
			p.setPen(Qt::NoPen); p.setBrush(QColor(124, 108, 255, 235));
			p.drawRoundedRect(tr, 10, 10);
			p.setPen(QColor(255, 255, 255));
			p.drawText(tr, Qt::AlignCenter, hint);

			// Tombol Capture & Cancel (muncul setelah ada seleksi).
			computeButtons();
			if (!captureBtn.isNull())
			{
				auto drawBtn = [&](const QRect& r, const QColor& bg, const QString& text)
				{
					p.setPen(Qt::NoPen); p.setBrush(bg); p.drawRoundedRect(r, 9, 9);
					p.setPen(QColor(255, 255, 255)); p.drawText(r, Qt::AlignCenter, text);
				};
				drawBtn(captureBtn, QColor(124, 108, 255), QString::fromUtf8(u8"\u2713  Capture"));
				drawBtn(cancelBtn, QColor(70, 74, 100), QString::fromUtf8(u8"\u2715  Cancel"));
			}
		}
		void finish(const QRect& widgetSel)
		{
			QRect global(widgetSel.topLeft() + virtualOrigin, widgetSel.size());
			auto cb = onDone; close();
			if (widgetSel.width() > 8 && widgetSel.height() > 8) cb(global);
			else cb(QRect());
		}
		void mousePressEvent(QMouseEvent* e) override
		{
			// Bila menekan tombol yang tampil -> aksi tombol (bukan mulai drag baru).
			if (!captureBtn.isNull() && captureBtn.contains(e->pos())) { finish(sel); return; }
			if (!cancelBtn.isNull() && cancelBtn.contains(e->pos())) { finish(QRect()); return; }
			origin = e->pos(); sel = QRect(origin, QSize()); hasSelection = false; update();
		}
		void mouseMoveEvent(QMouseEvent* e) override
		{
			if (e->buttons() & Qt::LeftButton) { sel = QRect(origin, e->pos()).normalized(); hasSelection = true; update(); }
		}
		void mouseReleaseEvent(QMouseEvent* e) override
		{
			// Setelah drag: JANGAN langsung capture. Tampilkan tombol Capture/Cancel.
			sel = QRect(origin, e->pos()).normalized();
			hasSelection = sel.width() > 8 && sel.height() > 8;
			update();
		}
		void keyPressEvent(QKeyEvent* e) override
		{
			if (e->key() == Qt::Key_Escape) finish(QRect());
			else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && hasSelection) finish(sel);
		}
	private:
		std::function<void(QRect)> onDone;
		QRubberBand rubber; // (tidak dipakai lagi; digambar manual)
		QPoint origin, virtualOrigin;
		QRect sel;                 // kotak seleksi (koordinat widget)
		bool hasSelection = false; // sudah ada kotak (tampilkan tombol)
		QRect captureBtn, cancelBtn;
	};

	bool SentenceReceived(TextThread& thread, std::wstring& sentence)
	{
		for (int i = 0; i < sentence.size(); ++i) if (sentence[i] == '\r' && sentence[i + 1] == '\n') sentence[i] = 0x200b; // for some reason \r appears as newline - no need to double

		// Bersihkan teks dari sumber nyata (game/LunaHook): buang kode kontrol & pengulangan.
		if (thread.tp.processId != 0 && tpCleanEnabled) CleanHookText(sentence);

		// Filter panjang: abaikan teks terlalu pendek/panjang (Text Processing).
		if (thread.tp.processId != 0)
		{
			int L = (int)sentence.size();
			if (L < tpMinLen || (tpMaxLen > 0 && L > tpMaxLen)) { sentence.clear(); return false; }
		}

		// Dedup baris berturut-turut yang IDENTIK dari thread yang sama. Banyak engine
		// mengeluarkan nama pembicara / baris yang sama dua kali (mis. "sophia" lalu
		// "sophia" lagi) -> tampil dobel. Lewati bila persis sama dengan baris sebelumnya.
		if (thread.tp.processId != 0 && !sentence.empty())
		{
			// (a) Dedup berurutan per-thread.
			static std::unordered_map<int64_t, std::wstring> lastSentencePerThread;
			auto& last = lastSentencePerThread[thread.handle];
			if (sentence == last) { sentence.clear(); return false; }
			last = sentence;

			// (b) Dedup GLOBAL jendela-waktu: banyak engine mengirim nama pembicara
			//     (mis. "カイ") dari thread berbeda / berulang sehingga tampil dobel.
			//     Bila teks yang SAMA persis sudah masuk < 4 dtk lalu (thread mana pun),
			//     lewati. Dialog nyata yang diulang > 4 dtk tetap tampil.
			static std::unordered_map<std::wstring, unsigned long long> recentSeen;
			unsigned long long nowTick = GetTickCount64();
			// bersihkan entri lama sesekali agar map tak membengkak.
			if (recentSeen.size() > 64)
				for (auto it = recentSeen.begin(); it != recentSeen.end(); )
					it = (nowTick - it->second > 8000) ? recentSeen.erase(it) : std::next(it);
			auto rs = recentSeen.find(sentence);
			if (rs != recentSeen.end() && nowTick - rs->second < 4000) { sentence.clear(); return false; }
			recentSeen[sentence] = nowTick;
		}

		// === VN Translator Auto Detect (berbasis pola dialog) ===
		// Pilih thread dengan KUALITAS DIALOG RATA-RATA tertinggi (bukan yang paling
		// banyak teks) supaya thread berisi teks sampah panjang tidak menang.
		// Hitung teks yang datang dari sumber nyata (game / LunaHook), untuk auto-fallback.
		if (thread.tp.processId != 0) sentencesSinceAttach++;

		// Abaikan thread OCR di pipeline hook (OCR ditangani terpisah di HandleOcrText,
		// tidak lewat SentenceReceived) supaya tidak mengganggu sumber hook.
		if (thread.name == L"OCR") return false;

		if (autoPickThread && thread.tp.processId != 0)
		{
			int q = ScoreDialogText(sentence);
			// simpan rata-rata bergerak kualitas + jumlah sampel di 32 bit terpisah:
			// gunakan map skor = rata-rata*100, dan map sampel terpisah.
			static std::unordered_map<int64_t, int> sampleCount;
			int& avg = threadDialogScore[thread.handle]; // menyimpan rata-rata (0..100)
			int& n = sampleCount[thread.handle];
			n++;
			avg = (avg * (n - 1) + q) / n; // rata-rata bergerak kualitas dialog thread ini

			// Skor thread yang sedang terpilih (real-time dari map), supaya perbandingan
			// selalu adil dan thread benar bisa menyalip thread yang keliru.
			int chosenAvg = (autoChosenHandle && threadDialogScore.count(autoChosenHandle))
				? threadDialogScore[autoChosenHandle] : -1;

			// Apakah thread terpilih SEDANG DIAM (tidak keluarkan teks) terlalu lama?
			// Penting saat game dipercepat/skip: dialog nyata bisa pindah ke thread lain,
			// sedangkan thread lama membeku. Bila diam >2.5 dtk, anggap "kedaluwarsa"
			// sehingga thread dialog yang aktif boleh mengambil alih tanpa harus menyalip
			// rata-rata lama (yang tidak pernah meluruh).
			unsigned long long lastTick = lastCurrentTextTick.load();
			bool incumbentStale = (autoChosenHandle != 0) && lastTick != 0
				&& (GetTickCount64() - lastTick > 2500);

			// Kandidat layak. Bila BELUM ada thread terpilih (atau incumbent sudah diam
			// terlalu lama), terima pada sampel yang jelas dialog (q tinggi) -> teks
			// langsung muncul tanpa nunggu. Selain itu, butuh rata-rata bagus + 2 sampel.
			bool freshPick = (autoChosenHandle == 0) || incumbentStale;
			bool qualifies = freshPick
				? (q >= 45)                       // baris meyakinkan -> langsung pakai
				: (avg >= 35 && n >= 2 && q >= 25);
			// Pindah bila: fresh-pick (belum ada / incumbent diam), ATAU jelas lebih baik.
			bool better = freshPick || (avg > chosenAvg + 6);
			if (qualifies && better && thread.handle != autoChosenHandle)
			{
				autoChosenHandle = thread.handle;
				autoChosenScore = avg;
				lastCurrentTextTick = GetTickCount64(); // baru dipilih -> jangan langsung dianggap diam
				int64_t chosenHandle = thread.handle;
				QMetaObject::invokeMethod(This, [chosenHandle]
				{
					if (TextThread* chosen = Host::GetThread(chosenHandle))
					{
						int idx = ui.ttCombo->findText(TextThreadString(*chosen), Qt::MatchStartsWith);
						if (idx >= 0) ViewThread(idx);
						current = chosen;
						SetHeaderStatus("Connected", true);
						if (simpleStatus) simpleStatus->setText(QString::fromUtf8(u8"Dialogue text source detected automatically."));
					}
				});
			}
		}

		// Tampilkan teks ASLI SEGERA (sebelum terjemahan online yang lambat), supaya
		// teks original muncul instan & tidak menunggu ~detik ke server terjemahan.
		if (&thread == current && thread.tp.processId != 0)
		{
			lastCurrentTextTick = GetTickCount64(); // 'current' aktif -> reset pewaktu diam
			QMetaObject::invokeMethod(This, [orig = S(sentence)]
			{
				if (srcText) SimpleSetOriginal(orig);
			});
		}

		// === Terjemahan ASINKRON ===
		// Jalankan terjemahan (DispatchSentenceToExtensions -> HTTP) di thread pekerja
		// agar kalimat berikutnya TIDAK menunggu terjemahan kalimat sebelumnya. Tiap
		// kalimat diberi nomor urut; kartu hanya diperbarui bila hasilnya TERBARU
		// (mencegah hasil lama menimpa hasil baru). History & overlay tetap terisi.
		bool isCurrent = (&thread == current);
		uint64_t seq = ++translationSeq;

		// Batasi konkuransi agar tidak kena rate-limit Google (maks ~4 request paralel).
		// Bila terlalu banyak berjalan, proses sinkron saja (jarang terjadi).
		bool runAsync = activeTranslations.load() < 4;

		auto doTranslate = [seq, isCurrent](TextThread* th, std::wstring s)
		{
			activeTranslations++;
			if (DispatchSentenceToExtensions(s, GetSentenceInfo(*th).data()))
			{
				s += L'\n';
				th->storage->append(s); // simpan ke riwayat thread (agar ViewThread tetap benar)
				bool current = isCurrent;
				QMetaObject::invokeMethod(This, [s = S(s), current, seq]() mutable
				{
					sanitize(s);
					// Advanced text view: selalu tambah (urutan tiba).
					auto scrollbar = ui.textOutput->verticalScrollBar();
					bool atBottom = scrollbar->value() + 3 > scrollbar->maximum() || (double)scrollbar->value() / scrollbar->maximum() > 0.975;
					QTextCursor cursor(ui.textOutput->document());
					cursor.movePosition(QTextCursor::End);
					cursor.insertText(s);
					if (atBottom) scrollbar->setValue(scrollbar->maximum());

					if (current && srcText)
					{
						QString sep = QString::fromUtf8(u8"\u200b \n");
						QString original, translation;
						if (s.contains(sep)) { auto parts = s.split(sep); original = parts.value(0).trimmed(); translation = parts.value(1).trimmed(); }
						else translation = s.trimmed();
						// Kartu hanya diperbarui bila ini hasil TERBARU (hindari hasil lama menimpa).
						if (seq >= lastShownSeq)
						{
							lastShownSeq = seq;
							SimpleAddPair(original, translation);
						}
						else if (!translation.isEmpty())
						{
							// hasil lama: tetap masukkan ke history saja (tanpa mengubah kartu).
							SimpleAddHistoryOnly(original, translation);
						}
					}
				});
			}
			activeTranslations--;
		};

		if (runAsync)
		{
			TextThread* th = &thread;
			std::wstring s = sentence;
			std::thread([doTranslate, th, s]() mutable { doTranslate(th, std::move(s)); }).detach();
		}
		else
		{
			doTranslate(&thread, sentence);
		}
		return false; // kita tangani penyimpanan/tampilan sendiri; host tak perlu menyimpan lagi
	}

	void OutputContextMenu(QPoint point)
	{
		std::unique_ptr<QMenu> menu(ui.textOutput->createStandardContextMenu());
		menu->addAction(FONT, [] { if (QString font = QFontDialog::getFont(&ok, ui.textOutput->font(), This, FONT).toString(); ok) SetOutputFont(font); });
		menu->exec(ui.textOutput->mapToGlobal(point));
	}

	void CopyUnlessMouseDown()
	{
		if (!(QApplication::mouseButtons() & Qt::LeftButton)) ui.textOutput->copy();
	}

	// --- VN Translator: Simple Mode ---

	// Cek apakah proses target 32-bit (WOW64) pada Windows 64-bit.
	bool ProcessIs32Bit(DWORD processId)
	{
		if (AutoHandle<> process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId))
		{
			BOOL wow64 = FALSE;
			IsWow64Process(process, &wow64);
			return wow64;
		}
		return false;
	}

	// Jalankan helper exe dengan arsitektur yang cocok untuk hook game beda-bit.
	// Bagi pengguna tetap "satu aplikasi": app ini yang menjalankan helper otomatis.
	bool LaunchHelperForArch(DWORD processId, bool need32Bit)
	{
		// Lokasi exe helper relatif terhadap exe sekarang.
		std::wstring self = GetModuleFilename().value();
		std::filesystem::path dir = std::filesystem::path(self).parent_path();
		std::filesystem::path exeName = std::filesystem::path(self).filename();

		std::filesystem::path helper;
		if (need32Bit)
			// GUI x64 -> butuh x86. exe x86 ada di folder induk (root paket).
			helper = dir.parent_path() / exeName;
		else
			// GUI x86 -> butuh x64. exe x64 ada di subfolder "x64".
			helper = dir / "x64" / exeName;

		if (!std::filesystem::exists(helper))
		{
			// fallback: coba nama Textractor.exe bila VN Translator.exe tak ada
			auto alt = helper;
			alt.replace_filename(L"Textractor.exe");
			if (std::filesystem::exists(alt)) helper = alt;
			else return false;
		}

		std::wstring cmd = L"\"" + helper.wstring() + L"\" -p" + std::to_wstring(processId);
		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
		cmdBuf.push_back(0);
		if (CreateProcessW(helper.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr,
			helper.parent_path().wstring().c_str(), &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			return true;
		}
		return false;
	}

	// Jalankan versi x64 (subfolder "x64") dengan flag -luna<pid> agar LunaHook
	// otomatis aktif di sana. Dipakai saat aplikasi x86 tak bisa memuat LunaHost64.dll.
	bool LaunchX64ForLuna(DWORD processId)
	{
		std::wstring self = GetModuleFilename().value();
		std::filesystem::path dir = std::filesystem::path(self).parent_path();
		std::filesystem::path exeName = std::filesystem::path(self).filename();
		std::filesystem::path helper = dir / "x64" / exeName;
		if (!std::filesystem::exists(helper))
		{
			auto alt = helper; alt.replace_filename(L"Textractor.exe");
			if (std::filesystem::exists(alt)) helper = alt; else return false;
		}
		std::wstring cmd = L"\"" + helper.wstring() + L"\" -luna" + std::to_wstring(processId);
		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
		cmdBuf.push_back(0);
		if (CreateProcessW(helper.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr,
			helper.parent_path().wstring().c_str(), &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			return true;
		}
		return false;
	}

	void SimpleSetStatus(const QString& text)
	{
		if (simpleStatus) simpleStatus->setText(text);
	}

	void SetHeaderStatus(const QString& text, bool connected)
	{
		if (headerStatus) headerStatus->setText((connected ? QString::fromUtf8(u8"\u25CF ") : QString::fromUtf8(u8"\u25CB ")) + text);
		if (hookDot) hookDot->setStyleSheet(connected ? "color:#22c55e;" : "color:#c4c4d4;");
	}

	// Perbarui badge mesin aktif di header (Textractor bawaan atau LunaHook).
	void SetEngineBadge(bool luna)
	{
		if (!engineBadge) return;
		engineBadge->setText(luna ? QString::fromUtf8(u8"\u26A1 Engine: LunaHook")
			: QString::fromUtf8(u8"Engine: Textractor"));
		engineBadge->setProperty("luna", luna);
		// refresh style (property selector)
		engineBadge->style()->unpolish(engineBadge);
		engineBadge->style()->polish(engineBadge);
		// Perbarui label tombol switch sesuai mesin aktif.
		if (engineSwitchBtn)
			engineSwitchBtn->setText(luna ? QString::fromUtf8(u8"\u21A9  Switch to Textractor")
				: QString::fromUtf8(u8"\u26A1  Switch to LunaHook"));
	}

	// Render teks asli Jepang dengan tiap segmen (kata kamus / kanji) sebagai tautan.
	QString RenderOriginalWithLinks(const QString& original)
	{
		auto& jl = JapaneseLearning::Instance();
		bool furigana = Settings().value("Japanese Learning/Show furigana", false).toBool();
		QString html;
		html.reserve(original.size() * 8);
		int i = 0, n = original.size();
		while (i < n)
		{
			QChar c = original.at(i);
			if (JapaneseLearning::IsKanji(c) || JapaneseLearning::IsKana(c))
			{
				int len = jl.MatchWordLengthAt(original, i, 8);
				QString seg = original.mid(i, len);
				if (len >= 2)
				{
					auto wi = jl.LookupWord(seg);
					html += QString("<a style='color:#1b2140; text-decoration:none;' href='%1'>%2</a>")
						.arg(("w:" + seg).toHtmlEscaped(), seg.toHtmlEscaped());
					if (furigana && wi.found && !wi.reading.isEmpty())
						html += QString("<span style='color:#8a8aa0; font-size:11px;'>(%1)</span>")
							.arg(wi.reading.toHtmlEscaped());
					// Beri spasi antar-kata saat furigana aktif -> lebih mudah dibaca.
					if (furigana) html += "<span style='font-size:11px;'> </span>";
					i += len;
				}
				else
				{
					html += QString("<a style='color:#1b2140; text-decoration:none;' href='%1'>%2</a>")
						.arg((QString("w:") + c).toHtmlEscaped(), QString(c).toHtmlEscaped());
					i += 1;
				}
			}
			else
			{
				if (c == '\n') html += "<br>";
				else html += QString(c).toHtmlEscaped();
				i += 1;
			}
		}
		return html;
	}

	// Tampilkan info kamus untuk segmen (kata atau satu kanji) di side panel.
	void ShowDictionary(const QString& segment)
	{
		auto& jl = JapaneseLearning::Instance();
		if (!dictPanel || segment.isEmpty()) return;

		QString word = segment;
		if (word.startsWith("w:")) word = word.mid(2);

		auto wi = jl.LookupWord(word);
		dictCurrentWord = word;
		dictCurrentReading = wi.found ? wi.reading : QString();
		dictCurrentJlpt = wi.found ? wi.jlpt : 0;

		QString html;
		html += QString("<div style='font-size:22px; color:#ffffff; font-weight:bold;'>%1</div>").arg(word.toHtmlEscaped());
		if (wi.found && !wi.reading.isEmpty())
			html += QString("<div style='color:#9b8cff; font-size:14px;'>%1</div>").arg(wi.reading.toHtmlEscaped());
		if (wi.found && wi.jlpt >= 1 && wi.jlpt <= 5)
			html += QString("<div style='color:#22c55e; font-size:12px; margin-top:2px;'>JLPT %1</div>")
				.arg(JapaneseLearning::JlptLabel(wi.jlpt));

		QStringList kanjiMeaningsForSave;
		QString kanjiHtml;
		for (QChar c : word)
		{
			if (!JapaneseLearning::IsKanji(c)) continue;
			auto ki = jl.LookupKanji(c);
			if (!ki.found) continue;
			QString meanings = ki.meanings.mid(0, 3).join(", ").toHtmlEscaped();
			if (!meanings.isEmpty()) kanjiMeaningsForSave << meanings;
			QString jl2 = JapaneseLearning::JlptLabel(ki.jlpt);
			kanjiHtml += QString("<div style='margin-top:10px; padding-top:8px; border-top:1px solid #262b45;'>"
				"<span style='font-size:20px; color:#e7e7f5;'>%1</span> "
				"<span style='color:#8a8aa0; font-size:11px;'>%2</span><br>"
				"<span style='color:#cfc9ff; font-size:12px;'>%3</span><br>"
				"<span style='color:#7f86ad; font-size:11px;'>On: %4 &nbsp; Kun: %5</span></div>")
				.arg(QString(c).toHtmlEscaped(),
					jl2.isEmpty() ? QString() : ("JLPT " + jl2),
					meanings,
					ki.onReadings.mid(0, 4).join(", ").toHtmlEscaped(),
					ki.kunReadings.mid(0, 4).join(", ").toHtmlEscaped());
		}
		if (kanjiHtml.isEmpty() && !wi.found)
			html += QString("<div style='color:#8a8aa0; font-size:12px; margin-top:8px;'>No dictionary entry found.</div>");
		html += kanjiHtml;

		dictCurrentMeaning = kanjiMeaningsForSave.join("; ");
		if (dictContent) dictContent->setText(html);
		dictPanel->setVisible(true);
	}

	// Render ulang kartu Original sesuai setelan belajar (mis. setelah toggle furigana).
	void RefreshLearningUI()
	{
		if (!srcText) return;
		bool clickable = Settings().value("Japanese Learning/Dictionary on click", true).toBool()
			&& JapaneseLearning::Instance().IsAvailable();
		if (clickable && !lastOriginal.isEmpty())
			srcText->setText(RenderOriginalWithLinks(lastOriginal));
		else
			srcText->setText(lastOriginal.isEmpty() ? QString::fromUtf8(u8"\u2014") : lastOriginal);
	}

	// Tampilkan HANYA teks asli segera (tanpa menunggu terjemahan & tanpa menambah history).
	// Kartu terjemahan diberi indikator "menerjemahkan..." sampai hasil datang.
	void SimpleSetOriginal(const QString& original)
	{
		lastOriginal = original;
		if (srcText)
		{
			bool clickable = Settings().value("Japanese Learning/Dictionary on click", true).toBool()
				&& JapaneseLearning::Instance().IsAvailable();
			if (clickable && !original.isEmpty())
				srcText->setText(RenderOriginalWithLinks(original));
			else
				srcText->setText(original.isEmpty() ? QString::fromUtf8(u8"\u2014") : original);
		}
		// Indikator terjemahan sedang berjalan (lebih jelas daripada sekadar titik).
		if (dstText) dstText->setText(QString::fromUtf8(u8"\u23F3 translating\u2026"));
	}

	// Tampilkan pasangan teks asli + terjemahan di kartu & history.
	void SimpleAddPair(const QString& original, const QString& translation)
	{
		lastOriginal = original;
		if (srcText)
		{
			// Bila fitur belajar aktif & data termuat, render teks asli dengan tautan
			// per-kata/kanji (klik -> kamus). Bila tidak, teks biasa.
			bool clickable = Settings().value("Japanese Learning/Dictionary on click", true).toBool()
				&& JapaneseLearning::Instance().IsAvailable();
			if (clickable && !original.isEmpty())
				srcText->setText(RenderOriginalWithLinks(original));
			else
				srcText->setText(original.isEmpty() ? QString::fromUtf8(u8"\u2014") : original);
		}
		if (dstText) dstText->setText(translation.isEmpty() ? QString::fromUtf8(u8"\u2026") : translation);
		SimpleAddHistoryOnly(original, translation);
	}

	// Tambah entri ke HISTORY saja (tanpa mengubah kartu Original/terjemahan).
	// Dipakai untuk hasil terjemahan lama (out-of-order) agar tidak menimpa kartu terbaru.
	void SimpleAddHistoryOnly(const QString& original, const QString& translation)
	{
		if (translation.isEmpty() || !historyLayout) return;
		statTranslated++;
		auto entry = new QWidget();
		entry->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		auto el = new QVBoxLayout(entry);
		el->setContentsMargins(10, 8, 10, 8); el->setSpacing(2);
		entry->setStyleSheet("background:#f4f4fb; border-radius:8px;");
		auto o = new QLabel(original); o->setWordWrap(true);
		o->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); o->setMinimumWidth(1);
		o->setStyleSheet("color:#8a8aa0; font-size:11px; background:transparent;");
		auto t = new QLabel(translation); t->setWordWrap(true);
		t->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); t->setMinimumWidth(1);
		t->setStyleSheet("color:#1b2140; font-size:13px; background:transparent;");
		el->addWidget(o); el->addWidget(t);
		historyLayout->insertWidget(0, entry); // terbaru di atas
		while (historyLayout->count() > 30)
		{
			auto item = historyLayout->takeAt(historyLayout->count() - 1);
			if (item->widget()) item->widget()->deleteLater();
			delete item;
		}
	}

	void SimplePickGame()
	{
		// Dialog pilih proses yang sama seperti Advanced, tapi alurnya diringkas.
		QMultiHash<QString, DWORD> processesMap;
		std::vector<std::pair<QString, HICON>> processIcons;
		for (auto [processId, processName] : GetAllProcesses())
		{
			if (processName && (showSystemProcesses || processName->find(L":\\Windows\\") == std::string::npos))
			{
				QString fileName = QFileInfo(S(processName.value())).fileName();
				if (!processesMap.contains(fileName))
				{
					HICON bigIcon, smallIcon;
					ExtractIconExW(processName->c_str(), 0, &bigIcon, &smallIcon, 1);
					processIcons.push_back({ fileName, bigIcon ? bigIcon : smallIcon });
				}
				processesMap.insert(fileName, processId);
			}
		}
		std::sort(processIcons.begin(), processIcons.end(), [](auto one, auto two) { return QString::compare(one.first, two.first, Qt::CaseInsensitive) < 0; });

		AttachProcessDialog dialog(This, processIcons);
		if (!dialog.exec()) return;

		QString process = dialog.SelectedProcess();
		QString gameLabel = process; // untuk ditampilkan
		std::vector<DWORD> targets;
		if (int pid = process.toInt(nullptr, 0)) targets.push_back(pid);
		else for (DWORD pid : processesMap.values(process)) targets.push_back(pid);
		if (targets.empty()) return;

		if (gameNameLabel) gameNameLabel->setText(QString::fromUtf8(u8"\U0001F3AE  ") + gameLabel);

		// Simpan nama exe game (untuk Game Profiles). Bila 'process' adalah PID, ambil nama exe-nya.
		currentGameExe.clear();
		{
			DWORD firstPid = targets.empty() ? 0 : targets.front();
			QString exe = QFileInfo(S(GetModuleFilename(firstPid).value_or(L""))).fileName();
			if (exe.isEmpty() && !process.toInt(nullptr, 0)) exe = process; // 'process' sudah nama file
			currentGameExe = exe;
		}
		if (saveGameBtn) saveGameBtn->setVisible(!currentGameExe.isEmpty());

		// reset auto-detect + mesin
		threadDialogScore.clear();
		autoChosenHandle = 0;
		autoChosenScore = -1;
		lastCurrentTextTick = 0;
		autoPickThread = true; // mode simpel: pilih sumber teks otomatis (Auto Detect)
		sentencesSinceAttach = 0;
		lunaEngineActive = false;
		lunaTargetPid = 0;
		SetEngineBadge(false); // mulai dengan mesin Textractor

		DWORD primaryPid = 0;
		for (DWORD pid : targets)
		{
			bool game32 = ProcessIs32Bit(pid);
			bool arch32 = !x64; // bitness GUI ini
			if (game32 != arch32)
			{
				SimpleSetStatus(QString::fromUtf8(u8"Preparing the ") + (game32 ? "32-bit" : "64-bit") + u8" engine for this game\u2026");
				if (!LaunchHelperForArch(pid, game32))
					SimpleSetStatus(QString::fromUtf8(u8"Failed to prepare the matching architecture engine. Try running the other version."));
			}
			else
			{
				Host::InjectProcess(pid);
				SetHeaderStatus("Connected", true);
				SimpleSetStatus(QString::fromUtf8(u8"Connected. Open a dialogue in the game \u2014 text will be translated automatically."));
				if (!primaryPid) primaryPid = pid;
			}
		}

		if (primaryPid)
		{
			lunaTargetPid = primaryPid;

			// Game Profile: bila game ini pernah disimpan dengan mesin LunaHook,
			// langsung pakai LunaHook (lewati fallback 8 detik).
			const GameProfiles::Profile* prof = currentGameExe.isEmpty() ? nullptr
				: GameProfiles::Instance().Find(currentGameExe);
			if (prof && prof->engine == "luna")
			{
				SimpleSetStatus(QString::fromUtf8(u8"Saved profile: using LunaHook for this game\u2026"));
				QTimer::singleShot(400, This, [primaryPid] { StartLunaEngine(primaryPid, true); });
			}
			else
			{
				// Auto-fallback ke LunaHook lebih cepat (~3.5 dtk) bila Textractor tak
				// menangkap teks - banyak engine (mis. Escude) memang tak terbaca Textractor,
				// jadi jangan buat pengguna menunggu lama. Tip: setelah LunaHook bekerja,
				// klik "Save this game" agar lain kali langsung pakai LunaHook tanpa jeda.
				QTimer::singleShot(3500, This, []
				{
					if (sentencesSinceAttach == 0 && !lunaEngineActive && lunaTargetPid)
					{
						SimpleSetStatus(QString::fromUtf8(u8"No text from Textractor \u2014 switching to LunaHook\u2026"));
						StartLunaEngine(lunaTargetPid, true);
					}
				});
			}
		}
	}

	// ===================== OCR (tangkap layar) =====================

	// Salurkan teks hasil OCR ke tampilan (kartu + history) TANPA mengganggu hook.
	// PENTING: OCR TIDAK memakai thread/current hook, jadi hook TIDAK terlepas -
	// teks hook tetap jalan; OCR hanya menampilkan hasilnya sendiri saat dipicu.
	// Terjemahan dijalankan langsung (async) via ekstensi, lalu ditampilkan.
	// ---- Folder penyimpanan hasil OCR (per game yang di-hook) ----
	QString OcrRootDir()
	{
		return QCoreApplication::applicationDirPath() + "/OCR";
	}
	QString OcrGameDir()
	{
		// Nama folder dari exe game yang sedang di-hook; bila belum ada -> "General".
		QString game = currentGameExe.trimmed();
		if (game.isEmpty()) game = "General";
		// Buang ekstensi .exe & bersihkan karakter yang tak valid utk nama folder.
		if (game.endsWith(".exe", Qt::CaseInsensitive)) game.chop(4);
		game.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
		if (game.isEmpty()) game = "General";
		return OcrRootDir() + "/" + game;
	}

	// Buat gambar hasil: screenshot di atas + panel terjemahan di bawah, jadi saat
	// dibuka lagi dari History langsung terlihat "menu ini artinya apa" pada gambarnya.
	QImage ComposeOcrImage(const QImage& shot, const QString& original, const QString& translation)
	{
		QImage top = shot.convertToFormat(QImage::Format_ARGB32);
		int w = qMax(top.isNull() ? 480 : top.width(), 480);

		// Ukur tinggi panel teks (wrap sesuai lebar).
		QFont fLbl; fLbl.setPointSize(9); fLbl.setBold(true);
		QFont fOrig("Meiryo", 12); QFont fTr; fTr.setPointSize(12); fTr.setBold(true);
		int pad = 14, gap = 6;
		int textW = w - pad * 2;
		auto measure = [&](const QFont& f, const QString& s)
		{
			QFontMetrics fm(f);
			QRect br = fm.boundingRect(QRect(0, 0, textW, 100000), Qt::TextWordWrap, s.isEmpty() ? " " : s);
			return br.height();
		};
		int hOrigLbl = QFontMetrics(fLbl).height();
		int hOrig = measure(fOrig, original);
		int hTrLbl = QFontMetrics(fLbl).height();
		int hTr = measure(fTr, translation.isEmpty() ? QString::fromUtf8(u8"(no translation)") : translation);
		int panelH = pad + hOrigLbl + gap + hOrig + pad + hTrLbl + gap + hTr + pad;

		int topH = top.isNull() ? 0 : top.height();
		QImage out(w, topH + panelH, QImage::Format_ARGB32);
		out.fill(QColor(24, 26, 38));

		QPainter p(&out);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::TextAntialiasing);
		if (!top.isNull())
		{
			// Tengahkan screenshot bila lebih sempit dari panel.
			p.drawImage(QPoint((w - top.width()) / 2, 0), top);
		}
		int y = topH + pad;
		// Label + teks asli.
		p.setFont(fLbl); p.setPen(QColor(150, 156, 181));
		p.drawText(QRect(pad, y, textW, hOrigLbl), Qt::AlignLeft, QString::fromUtf8(u8"Original"));
		y += hOrigLbl + gap;
		p.setFont(fOrig); p.setPen(QColor(232, 235, 245));
		p.drawText(QRect(pad, y, textW, hOrig), Qt::TextWordWrap, original);
		y += hOrig + pad;
		// Label + terjemahan (disorot).
		p.setFont(fLbl); p.setPen(QColor(150, 156, 181));
		p.drawText(QRect(pad, y, textW, hTrLbl), Qt::AlignLeft, QString::fromUtf8(u8"Translation"));
		y += hTrLbl + gap;
		p.setFont(fTr); p.setPen(QColor(140, 170, 255));
		p.drawText(QRect(pad, y, textW, hTr), Qt::TextWordWrap,
			translation.isEmpty() ? QString::fromUtf8(u8"(no translation)") : translation);
		p.end();
		return out;
	}

	// Simpan satu sesi OCR: gambar hasil (screenshot+terjemahan) PNG + JSON + TXT.
	void SaveOcrSession(const QImage& shot, const QString& original, const QString& translation)
	{
		if (shot.isNull() && original.trimmed().isEmpty()) return;
		QString dir = OcrGameDir();
		QDir().mkpath(dir);
		QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
		QString base = dir + "/ocr_" + stamp;

		// 1) Gambar hasil = screenshot + panel terjemahan (dibuka & jadi thumbnail).
		QImage composed = ComposeOcrImage(shot, original, translation);
		if (!composed.isNull()) composed.save(base + ".png", "PNG");
		// Simpan juga screenshot mentah (opsional, buat referensi).
		if (!shot.isNull()) shot.save(base + "_raw.png", "PNG");

		// 2) Metadata JSON (game, waktu, area, teks asli, terjemahan, sumber).
		QJsonObject o;
		o["game"] = currentGameExe.isEmpty() ? QString("General") : currentGameExe;
		o["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
		o["area"] = QJsonObject{ { "x", ocrRegion.x() }, { "y", ocrRegion.y() },
			{ "w", ocrRegion.width() }, { "h", ocrRegion.height() } };
		o["ocr"] = original;
		o["translation"] = translation;
		o["source"] = "OCR_MANUAL";
		o["image"] = QFileInfo(base + ".png").fileName();
		QFile jf(base + ".json");
		if (jf.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			jf.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
			jf.close();
		}

		// 3) TXT (mudah dicari / copy).
		QFile tf(base + ".txt");
		if (tf.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QTextStream ts(&tf);
			ts.setCodec("UTF-8");
			ts << "[OCR] " << original << "\n\n[Translation] " << translation << "\n";
			tf.close();
		}

		if (This) QMetaObject::invokeMethod(This, [] { RefreshOcrHistoryIfOpen(); });
	}

	// Entri utama hasil OCR dengan gambar: tampilkan + terjemah + SIMPAN sesi.
	void HandleOcrResult(const QString& text, const QImage& shot)
	{
		QString t = text.trimmed();
		if (t.isEmpty())
		{
			if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"No text detected in the selected area."));
			return;
		}
		if (t == lastOcrText) return; // dedup
		lastOcrText = t;

		// Tampilkan teks asli OCR segera.
		QString orig = t;
		QMetaObject::invokeMethod(This, [orig] { if (srcText) SimpleSetOriginal(orig); });
		if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"Translating\u2026"));

		QImage shotCopy = shot;
		std::thread([t, shotCopy]
		{
			TextThread& ocrTh = Host::GetLunaThread(0x0C4, 0x0C4, L"OCR");
			std::wstring s = t.toStdWString();
			QString original = t, translation;
			if (DispatchSentenceToExtensions(s, GetSentenceInfo(ocrTh).data()))
			{
				QString full = S(s);
				QString sep = QString::fromUtf8(u8"\u200b \n");
				if (full.contains(sep)) { auto parts = full.split(sep); original = parts.value(0).trimmed(); translation = parts.value(1).trimmed(); }
				else translation = full.trimmed();
				QString o = original, tr = translation;
				QMetaObject::invokeMethod(This, [o, tr]
				{
					SimpleAddPair(o, tr);
					if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"OCR saved to history."));
				});
			}
			// Simpan sesi (screenshot + teks + terjemahan), meski terjemahan kosong.
			SaveOcrSession(shotCopy, original, translation);
		}).detach();
	}

	// Versi teks-saja (dipakai jalur lama / auto OCR yg belum bawa gambar).
	void HandleOcrText(const QString& text)
	{
		HandleOcrResult(text, QImage());
	}

	// Pilih area layar untuk OCR (drag kotak). Simpan ke QSettings.
	void PickOcrRegion()
	{
		if (!OcrEngine::Instance().IsAvailable())
		{
			if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"OCR not available on this system."));
			return;
		}
		// P1 proposal: sembunyikan jendela utama SEBELUM overlay muncul agar tidak
		// ikut ter-capture / menutupi game.
		// PENTING: jendela utama TETAP tersembunyi sampai capture selesai, baru
		// di-restore. Kalau di-show() lebih dulu, window ikut ter-capture (bug).
		if (This) This->hide();
		auto picker = new OcrRegionPicker([](QRect r)
		{
			if (r.isNull())
			{
				// Batal: langsung kembalikan window.
				if (This) { This->show(); This->raise(); This->activateWindow(); }
				if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"OCR area selection cancelled."));
				return;
			}
			ocrRegion = r;
			Settings s; s.setValue("OCR/x", r.x()); s.setValue("OCR/y", r.y());
			s.setValue("OCR/w", r.width()); s.setValue("OCR/h", r.height());
			if (ocrNowBtn) ocrNowBtn->setEnabled(true);
			if (ocrAutoBtn) ocrAutoBtn->setEnabled(true);
			// Overlay picker sudah close(); window utama MASIH hidden. Beri jeda agar
			// keduanya benar-benar lepas dari layar, tangkap, LALU restore window.
			QTimer::singleShot(300, This, []
			{
				QRect r = ocrRegion;
				OcrEngine::Instance().RecognizeRegionAsync(r, [](QString text, QImage shot)
				{
					QMetaObject::invokeMethod(This, [text, shot]
					{
						// Restore window utama SETELAH capture (tidak ikut ter-capture).
						if (This) { This->show(); This->raise(); This->activateWindow(); }
						HandleOcrResult(text, shot);
					});
				});
			});
		});
		// Beri jeda kecil agar jendela hilang dari layar sebelum overlay tampil.
		QTimer::singleShot(150, [picker]
		{
			picker->show();
			picker->raise();
			picker->activateWindow();
		});
	}

	// OCR sekali pada region tersimpan.
	void RunOcrOnce()
	{
		if (ocrRegion.isNull()) { if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"Pick an OCR area first.")); return; }
		QRect r = ocrRegion;
		// Ambil TEKS + SCREENSHOT dari satu capture, lalu tampilkan + simpan sesi.
		OcrEngine::Instance().RecognizeRegionAsync(r, [](QString text, QImage shot)
		{
			QMetaObject::invokeMethod(This, [text, shot] { HandleOcrResult(text, shot); });
		});
	}

	// Hidupkan/matikan OCR berkala (tiap ~1.5 dtk) untuk menu yang berubah.
	void ToggleAutoOcr()
	{
		if (!ocrTimer)
		{
			ocrTimer = new QTimer(This);
			ocrTimer->setInterval(1500);
			QObject::connect(ocrTimer, &QTimer::timeout, [] { RunOcrOnce(); });
		}
		if (ocrTimer->isActive())
		{
			ocrTimer->stop();
			if (ocrAutoBtn) ocrAutoBtn->setText(QString::fromUtf8(u8"\u23F1  Auto OCR"));
			if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"Auto OCR stopped."));
		}
		else
		{
			if (ocrRegion.isNull()) { if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"Pick an OCR area first.")); return; }
			ocrTimer->start();
			if (ocrAutoBtn) ocrAutoBtn->setText(QString::fromUtf8(u8"\u23F9  Stop Auto OCR"));
			if (ocrStatus) ocrStatus->setText(QString::fromUtf8(u8"Auto OCR on (every 1.5s)."));
		}
	}

	// ===================== OCR History (side panel kanan) =====================

	// Bangun panel (sekali). Panel = QDockWidget di sisi kanan window utama.
	void BuildOcrHistoryPanel()
	{
		if (ocrHistoryPanel) return;
		auto dock = new QDockWidget(QString::fromUtf8(u8"OCR History"), This);
		dock->setObjectName("ocrHistoryDock");
		dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
		dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

		auto body = new QWidget(dock);
		auto v = new QVBoxLayout(body);
		v->setContentsMargins(8, 8, 8, 8);
		v->setSpacing(8);

		// Baris atas: pilih game + refresh.
		auto top = new QHBoxLayout();
		ocrHistoryGame = new QComboBox(body);
		ocrHistoryGame->setToolTip(QString::fromUtf8(u8"Choose a game session"));
		auto refreshBtn = new QToolButton(body);
		refreshBtn->setText(QString::fromUtf8(u8"\u21BB"));
		refreshBtn->setToolTip(QString::fromUtf8(u8"Refresh"));
		top->addWidget(ocrHistoryGame, 1);
		top->addWidget(refreshBtn, 0);
		v->addLayout(top);

		// Daftar thumbnail (icon mode grid).
		ocrHistoryList = new QListWidget(body);
		ocrHistoryList->setViewMode(QListView::IconMode);
		ocrHistoryList->setIconSize(QSize(150, 90));
		ocrHistoryList->setResizeMode(QListView::Adjust);
		ocrHistoryList->setSpacing(8);
		ocrHistoryList->setMovement(QListView::Static);
		ocrHistoryList->setWordWrap(true);
		ocrHistoryList->setContextMenuPolicy(Qt::CustomContextMenu);
		v->addWidget(ocrHistoryList, 1);

		auto hint = new QLabel(QString::fromUtf8(u8"Double-click an item to reopen it."), body);
		hint->setStyleSheet("color:#9aa0b5;");
		v->addWidget(hint, 0);

		dock->setWidget(body);
		This->addDockWidget(Qt::RightDockWidgetArea, dock);
		ocrHistoryPanel = dock;

		QObject::connect(refreshBtn, &QToolButton::clicked, [] { RefreshOcrHistoryGames(); RefreshOcrHistoryList(); });
		QObject::connect(ocrHistoryGame, QOverload<int>::of(&QComboBox::currentIndexChanged), [] { RefreshOcrHistoryList(); });
		QObject::connect(ocrHistoryList, &QListWidget::itemDoubleClicked, [](QListWidgetItem* it)
		{
			if (it) OpenOcrHistoryItem(it->data(Qt::UserRole).toString());
		});
		QObject::connect(ocrHistoryList, &QListWidget::customContextMenuRequested, [](const QPoint& pos)
		{
			QListWidgetItem* it = ocrHistoryList->itemAt(pos);
			if (!it) return;
			QMenu m;
			QAction* open = m.addAction(QString::fromUtf8(u8"Open"));
			QAction* del = m.addAction(QString::fromUtf8(u8"Delete"));
			QAction* chosen = m.exec(ocrHistoryList->mapToGlobal(pos));
			QString path = it->data(Qt::UserRole).toString();
			if (chosen == open) OpenOcrHistoryItem(path);
			else if (chosen == del) DeleteOcrHistoryItem(path);
		});
	}

	// Buka/tutup panel.
	void ToggleOcrHistoryPanel()
	{
		BuildOcrHistoryPanel();
		auto dock = qobject_cast<QDockWidget*>(ocrHistoryPanel);
		if (!dock) return;
		if (dock->isVisible()) { dock->hide(); return; }
		RefreshOcrHistoryGames();
		RefreshOcrHistoryList();
		dock->show();
		dock->raise();
	}

	// Isi combo daftar game dari sub-folder OCR/*.
	void RefreshOcrHistoryGames()
	{
		if (!ocrHistoryGame) return;
		QString prev = ocrHistoryGame->currentText();
		ocrHistoryGame->blockSignals(true);
		ocrHistoryGame->clear();
		QDir root(OcrRootDir());
		if (root.exists())
		{
			QStringList games = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
			// Prioritaskan game yang sedang di-hook di paling atas.
			QString cur = currentGameExe;
			if (cur.endsWith(".exe", Qt::CaseInsensitive)) cur.chop(4);
			if (!cur.isEmpty() && games.contains(cur)) { games.removeAll(cur); games.prepend(cur); }
			ocrHistoryGame->addItems(games);
		}
		// Pulihkan pilihan sebelumnya bila ada.
		int idx = ocrHistoryGame->findText(prev);
		if (idx >= 0) ocrHistoryGame->setCurrentIndex(idx);
		ocrHistoryGame->blockSignals(false);
		if (ocrHistoryGame->count() == 0) ocrHistoryGame->addItem(QString::fromUtf8(u8"(no OCR sessions yet)"));
	}

	// Isi daftar thumbnail untuk game terpilih (baca file *.json + *.png).
	void RefreshOcrHistoryList()
	{
		if (!ocrHistoryList || !ocrHistoryGame) return;
		ocrHistoryList->clear();
		QString game = ocrHistoryGame->currentText();
		if (game.isEmpty() || game.startsWith("(")) return;
		QDir dir(OcrRootDir() + "/" + game);
		if (!dir.exists()) return;
		// Urutkan terbaru dulu.
		QStringList jsons = dir.entryList(QStringList() << "ocr_*.json", QDir::Files, QDir::Name | QDir::Reversed);
		for (const QString& jn : jsons)
		{
			QString jpath = dir.absoluteFilePath(jn);
			QFile jf(jpath);
			QString original, time, imgName;
			if (jf.open(QIODevice::ReadOnly))
			{
				QJsonObject o = QJsonDocument::fromJson(jf.readAll()).object();
				jf.close();
				original = o["ocr"].toString();
				time = o["time"].toString();
				imgName = o["image"].toString();
			}
			QString imgPath = imgName.isEmpty() ? (jpath.left(jpath.size() - 5) + ".png") : dir.absoluteFilePath(imgName);
			QIcon icon;
			if (QFile::exists(imgPath))
			{
				QPixmap pm(imgPath);
				if (!pm.isNull()) icon = QIcon(pm.scaled(150, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
			}
			QString when = QDateTime::fromString(time, Qt::ISODate).toString("MM-dd HH:mm");
			QString label = when + "\n" + original.left(24).replace("\n", " ");
			auto item = new QListWidgetItem(icon, label);
			item->setData(Qt::UserRole, jpath);
			item->setToolTip(original);
			ocrHistoryList->addItem(item);
		}
	}

	void RefreshOcrHistoryIfOpen()
	{
		auto dock = qobject_cast<QDockWidget*>(ocrHistoryPanel);
		if (dock && dock->isVisible()) { RefreshOcrHistoryGames(); RefreshOcrHistoryList(); }
	}

	// Buka lagi sebuah sesi OCR: tampilkan gambar + teks + terjemahan dalam dialog.
	void OpenOcrHistoryItem(const QString& jsonPath)
	{
		if (jsonPath.isEmpty()) return;
		QFile jf(jsonPath);
		QString original, translation, imgName, game, time;
		if (jf.open(QIODevice::ReadOnly))
		{
			QJsonObject o = QJsonDocument::fromJson(jf.readAll()).object();
			jf.close();
			original = o["ocr"].toString();
			translation = o["translation"].toString();
			imgName = o["image"].toString();
			game = o["game"].toString();
			time = o["time"].toString();
		}
		QFileInfo fi(jsonPath);
		QString imgPath = imgName.isEmpty() ? (jsonPath.left(jsonPath.size() - 5) + ".png") : fi.dir().absoluteFilePath(imgName);

		auto dlg = new QDialog(This);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setWindowTitle(QString::fromUtf8(u8"OCR \u2014 ") + game + "  (" + QDateTime::fromString(time, Qt::ISODate).toString("yyyy-MM-dd HH:mm") + ")");
		dlg->resize(560, 620);
		auto v = new QVBoxLayout(dlg);

		if (QFile::exists(imgPath))
		{
			QPixmap pm(imgPath);
			if (!pm.isNull())
			{
				auto lbl = new QLabel(dlg);
				lbl->setAlignment(Qt::AlignCenter);
				lbl->setPixmap(pm.scaled(520, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation));
				auto sa = new QScrollArea(dlg);
				sa->setWidgetResizable(true);
				sa->setWidget(lbl);
				v->addWidget(sa, 3);
			}
		}
		auto oLbl = new QLabel(QString::fromUtf8(u8"<b>Original</b>"), dlg); v->addWidget(oLbl);
		auto oTxt = new QPlainTextEdit(original, dlg); oTxt->setReadOnly(true); oTxt->setMaximumHeight(110); v->addWidget(oTxt, 1);
		auto tLbl = new QLabel(QString::fromUtf8(u8"<b>Translation</b>"), dlg); v->addWidget(tLbl);
		auto tTxt = new QPlainTextEdit(translation, dlg); tTxt->setReadOnly(true); v->addWidget(tTxt, 2);

		auto btns = new QHBoxLayout();
		auto openFolder = new QPushButton(QString::fromUtf8(u8"Open folder"), dlg);
		auto closeBtn = new QPushButton(QString::fromUtf8(u8"Close"), dlg);
		btns->addWidget(openFolder); btns->addStretch(1); btns->addWidget(closeBtn);
		v->addLayout(btns);
		QObject::connect(openFolder, &QPushButton::clicked, [imgPath] { QProcess::startDetached("explorer", QStringList() << "/select," << QDir::toNativeSeparators(imgPath)); });
		QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
		dlg->show();
	}

	void DeleteOcrHistoryItem(const QString& jsonPath)
	{
		if (jsonPath.isEmpty()) return;
		if (QMessageBox::question(This, QString::fromUtf8(u8"Delete OCR item"),
			QString::fromUtf8(u8"Delete this OCR session (image + text)?")) != QMessageBox::Yes) return;
		QString base = jsonPath.left(jsonPath.size() - 5); // buang ".json"
		QFile::remove(base + ".json");
		QFile::remove(base + ".png");
		QFile::remove(base + "_raw.png");
		QFile::remove(base + ".txt");
		RefreshOcrHistoryList();
	}

	// Simpan profil game aktif: mesin (Textractor/LunaHook) + bahasa target.
	// Untuk Textractor, hook thread aktif juga disimpan lewat SaveHooks() (mekanisme bawaan)
	// sehingga saat game dibuka lagi hook langsung dipasang tanpa deteksi ulang.
	void SaveCurrentGameProfile()
	{
		if (currentGameExe.isEmpty())
		{
			SimpleSetStatus(QString::fromUtf8(u8"Pick a game first, then save it."));
			return;
		}
		GameProfiles::Profile p;
		p.exe = currentGameExe;
		p.title = currentGameExe;
		p.engine = lunaEngineActive ? "luna" : "textractor";
		p.targetLang = Settings().value("VN Translate/Translate to", "English").toString();
		// Pertahankan judul lama bila sudah ada.
		if (auto ex = GameProfiles::Instance().Find(currentGameExe)) p.title = ex->title;

		// Untuk mesin Textractor: simpan hook thread aktif (agar auto-restore & bisa di-share).
		if (!lunaEngineActive)
		{
			selectedProcessId = current ? current->tp.processId : selectedProcessId.load();
			SaveHooks();
			// Ambil kode hook thread aktif untuk disematkan ke profil (share).
			if (current && !(current->hp.type & HOOK_ENGINE))
				p.hookCode = S(HookCode::Generate(current->hp, current->tp.processId));
		}
		GameProfiles::Instance().Set(p);
		RefreshSavedGamesList();
		SimpleSetStatus(QString::fromUtf8(u8"Saved. Next time you open this game it starts automatically."));
	}

	// Ekspor semua profil game (termasuk hook code) ke file JSON untuk dibagikan.
	void ExportGameProfiles()
	{
		if (GameProfiles::Instance().All().isEmpty())
		{
			SimpleSetStatus(QString::fromUtf8(u8"No saved games to export yet."));
			return;
		}
		QString path = QFileDialog::getSaveFileName(This, QString::fromUtf8(u8"Export saved games"),
			"Shin_Translator_games.json", "Shin Translator profiles (*.json)");
		if (path.isEmpty()) return;
		// Pastikan berekstensi .json bila pengguna mengetik tanpa ekstensi.
		if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";
		int count = GameProfiles::Instance().All().size();
		if (GameProfiles::Instance().ExportToFile(path))
		{
			QFileInfo fi(path);
			SimpleSetStatus(QString::fromUtf8(u8"Exported ") + QString::number(count) + QString::fromUtf8(u8" game(s)."));
			QMessageBox box(This);
			box.setIcon(QMessageBox::Information);
			box.setWindowTitle(QString::fromUtf8(u8"Export successful"));
			box.setText(QString::fromUtf8(u8"Exported ") + QString::number(count) +
				QString::fromUtf8(u8" saved game(s) to:\n\n") + fi.fileName());
			box.setInformativeText(QString::fromUtf8(u8"Location: ") + QDir::toNativeSeparators(fi.absolutePath()) +
				QString::fromUtf8(u8"\n\nShare this file so others get the text instantly."));
			auto openBtn = box.addButton(QString::fromUtf8(u8"Open folder"), QMessageBox::ActionRole);
			box.addButton(QMessageBox::Ok);
			box.exec();
			if (box.clickedButton() == openBtn)
				QProcess::startDetached("explorer", QStringList() << "/select," << QDir::toNativeSeparators(path));
		}
		else
		{
			SimpleSetStatus(QString::fromUtf8(u8"Export failed."));
			QMessageBox::warning(This, QString::fromUtf8(u8"Export failed"),
				QString::fromUtf8(u8"Could not write the file. Check the folder permissions and try again."));
		}
	}

	// Impor profil game yang di-share (upsert). Hook code ikut sehingga teks langsung muncul.
	void ImportGameProfiles()
	{
		QString path = QFileDialog::getOpenFileName(This, QString::fromUtf8(u8"Import shared games"),
			".", "Shin Translator profiles (*.json);;All files (*.*)");
		if (path.isEmpty()) return;
		int n = GameProfiles::Instance().ImportFromFile(path);
		if (n < 0)
		{
			SimpleSetStatus(QString::fromUtf8(u8"Import failed: invalid file."));
			QMessageBox::warning(This, QString::fromUtf8(u8"Import failed"),
				QString::fromUtf8(u8"That file is not a valid Shin Translator profile file."));
			return;
		}
		RefreshSavedGamesList();
		SimpleSetStatus(QString::fromUtf8(u8"Imported ") + QString::number(n) + QString::fromUtf8(u8" game(s)."));
		QMessageBox::information(This, QString::fromUtf8(u8"Import successful"),
			QString::fromUtf8(u8"Imported ") + QString::number(n) +
			QString::fromUtf8(u8" game(s).\n\nOpen a saved game and the translated text will appear instantly."));
	}

	// Terapkan profil tersimpan (dipanggil dari daftar game / saat pilih game).
	void ApplyGameProfileIfAny(const QString& exe, DWORD pid)
	{
		auto prof = GameProfiles::Instance().Find(exe);
		if (!prof) return;
		if (prof->engine == "luna" && pid) QTimer::singleShot(400, This, [pid] { StartLunaEngine(pid, true); });
	}

	// Bangun ulang daftar game tersimpan di Simple Mode (dengan Edit & Delete).
	void RefreshSavedGamesList()
	{
		if (!savedGamesList) return;
		// bersihkan
		if (auto lay = savedGamesList->layout())
		{
			QLayoutItem* it;
			while ((it = lay->takeAt(0))) { if (it->widget()) it->widget()->deleteLater(); delete it; }
		}
		else
		{
			auto v = new QVBoxLayout(savedGamesList);
			v->setContentsMargins(0, 0, 0, 0); v->setSpacing(6);
		}
		auto lay = qobject_cast<QVBoxLayout*>(savedGamesList->layout());
		const auto& profs = GameProfiles::Instance().All();
		if (profs.isEmpty())
		{
			auto empty = new QLabel(QString::fromUtf8(u8"No saved games yet. Connect a game, then \u2b50 Save this game."));
			empty->setObjectName("cardHint"); empty->setWordWrap(true);
			lay->addWidget(empty);
			savedGamesList->setVisible(true);
			return;
		}
		for (const auto& p : profs)
		{
			auto row = new QWidget(savedGamesList); row->setObjectName("savedGameRow");
			auto rl = new QHBoxLayout(row); rl->setContentsMargins(12, 9, 10, 9); rl->setSpacing(8);
			QString eng = (p.engine == "luna") ? QString::fromUtf8(u8"\u26A1 LunaHook") : "Textractor";
			auto info = new QLabel(QString("<b style='color:#1b2140;'>%1</b>&nbsp;<span style='color:#8a8aa0; font-size:11px;'>%2 \u2022 %3</span>")
				.arg(p.title.toHtmlEscaped(), eng, p.targetLang.toHtmlEscaped()), row);
			info->setTextFormat(Qt::RichText); info->setStyleSheet("background:transparent;");
			rl->addWidget(info, 1);
			auto editBtn = new QPushButton(QString::fromUtf8(u8"\u270E"), row);
			editBtn->setObjectName("miniBtn"); editBtn->setFixedSize(26, 26); editBtn->setCursor(Qt::PointingHandCursor);
			editBtn->setToolTip("Rename");
			QString exe = p.exe, title = p.title;
			QObject::connect(editBtn, &QPushButton::clicked, [exe, title]
			{
				bool okp = false;
				QString name = QInputDialog::getText(This, QString::fromUtf8(u8"Rename game"), QString::fromUtf8(u8"Name:"), QLineEdit::Normal, title, &okp);
				if (okp && !name.isEmpty()) { GameProfiles::Instance().Rename(exe, name); RefreshSavedGamesList(); }
			});
			rl->addWidget(editBtn);
			auto delBtn = new QPushButton(QString::fromUtf8(u8"\u2715"), row);
			delBtn->setObjectName("miniDelBtn"); delBtn->setFixedSize(26, 26); delBtn->setCursor(Qt::PointingHandCursor);
			delBtn->setToolTip("Delete");
			QObject::connect(delBtn, &QPushButton::clicked, [exe]
			{
				GameProfiles::Instance().Remove(exe); RefreshSavedGamesList();
			});
			rl->addWidget(delBtn);
			lay->addWidget(row);
		}
		savedGamesList->setVisible(true);
	}

	// Badge kartu tujuan: ikuti bahasa target VN Translate (default English).
	// Mengembalikan "<emoji bendera>  <Nama Bahasa>".
	QString TargetLanguageBadge()
	{
		Settings s;
		s.beginGroup("VN Translate");
		QString lang = s.value("Translate to", "English").toString();
		s.endGroup();
		if (lang.isEmpty()) lang = "English";
		// Peta emoji bendera untuk bahasa umum; selain itu pakai ikon terjemahan.
		static const QHash<QString, QString> flags = {
			{ "English", QString::fromUtf8(u8"\U0001F1EC\U0001F1E7") },
			{ "Indonesian", QString::fromUtf8(u8"\U0001F1EE\U0001F1E9") },
			{ "Japanese", QString::fromUtf8(u8"\U0001F1EF\U0001F1F5") },
			{ "Korean", QString::fromUtf8(u8"\U0001F1F0\U0001F1F7") },
			{ "Chinese (Simplified)", QString::fromUtf8(u8"\U0001F1E8\U0001F1F3") },
			{ "Chinese (Traditional)", QString::fromUtf8(u8"\U0001F1F9\U0001F1FC") },
			{ "Spanish", QString::fromUtf8(u8"\U0001F1EA\U0001F1F8") },
			{ "French", QString::fromUtf8(u8"\U0001F1EB\U0001F1F7") },
			{ "German", QString::fromUtf8(u8"\U0001F1E9\U0001F1EA") },
			{ "Russian", QString::fromUtf8(u8"\U0001F1F7\U0001F1FA") },
			{ "Portuguese", QString::fromUtf8(u8"\U0001F1F5\U0001F1F9") },
			{ "Vietnamese", QString::fromUtf8(u8"\U0001F1FB\U0001F1F3") },
			{ "Thai", QString::fromUtf8(u8"\U0001F1F9\U0001F1ED") },
		};
		QString flag = flags.value(lang, QString::fromUtf8(u8"\U0001F310"));
		return flag + "  " + lang;
	}

	// Perbarui badge bahasa target di kartu tujuan (tanpa restart).
	void RefreshTargetBadge()
	{
		if (dstBadge) dstBadge->setText(TargetLanguageBadge());
	}

	void SetSimpleMode(bool on)
	{
		simpleMode = on;
		if (simplePanel) simplePanel->setVisible(on);
		if (advancedPanel) advancedPanel->setVisible(!on);
		Settings().setValue(SIMPLE_MODE, on);
	}

	// Muat LunaHost + daftarkan output handler (sekali saja).
	void EnsureLunaStarted()
	{
		if (lunaStarted) return;
		auto& luna = LunaBridge::Instance();
		// Teks dari LunaHook -> marshal ke thread Qt -> masuk pipeline lewat Host::AddLunaSentence,
		// sehingga melewati skoring + ekstensi terjemahan + kartu/overlay/history yang SAMA.
		luna.SetOutputHandler([](uint64_t ctx, uint64_t ctx2, const std::wstring& hookName, const std::wstring& text)
		{
			std::wstring name = hookName.empty() ? L"LunaHook" : (L"Luna:" + hookName);
			std::wstring t = text;
			QMetaObject::invokeMethod(This, [ctx, ctx2, name = std::move(name), t = std::move(t)]() mutable
			{
				Host::AddLunaSentence(ctx, ctx2, std::move(name), std::move(t));
			});
		});
		luna.SetInfoHandler([](int type, const std::wstring& message)
		{
			// Peringatan/log LunaHook -> konsol Textractor (terlihat di Advanced).
			if (!message.empty()) Host::AddConsoleOutput(L"[LunaHook] " + message);
		});
		if (!luna.EnsureStarted())
			Host::AddConsoleOutput(L"[LunaHook] " + luna.LastError());
		lunaStarted = true;
	}

	// Sambungkan + inject LunaHook untuk pid game. announce=true -> update status UI.
	void StartLunaEngine(DWORD processId, bool announce)
	{
		if (lunaEngineActive) return;
		EnsureLunaStarted();
		auto& luna = LunaBridge::Instance();
		if (!luna.IsAvailable())
		{
			// LunaHook (LunaHost64.dll) hanya jalan di proses 64-bit. Bila aplikasi ini
			// x86, jalankan versi x64 untuk game yang sama dengan flag -luna agar
			// LunaHook otomatis aktif di sana (pengguna tak perlu tahu soal x86/x64).
			if (!x64)
			{
				// Cegah menumpuk banyak jendela x64: hanya luncurkan sekali.
				if (lunaX64Launched)
				{
					if (announce && simpleStatus)
						simpleStatus->setText(QString::fromUtf8(u8"The 64-bit LunaHook window is already open. Use that window."));
					return;
				}
				if (LaunchX64ForLuna(processId))
				{
					lunaX64Launched = true;
					// Lepas attach Textractor dari game ini agar tidak bentrok dgn LunaHook,
					// lalu tutup jendela x86 SEGERA (tanpa delay panjang) supaya hanya
					// ada satu jendela (versi x64 dengan LunaHook).
					try { Host::DetachProcess(processId); } catch (...) {}
					QTimer::singleShot(150, This, []
					{
						CleanupExtensions();
						SetErrorMode(SEM_NOGPFAULTERRORBOX);
						ExitProcess(0);
					});
				}
				else if (announce && simpleStatus)
					simpleStatus->setText(QString::fromUtf8(u8"Could not start the 64-bit LunaHook engine. Run x64\\Shin Translator.exe."));
				return;
			}
			if (announce && simpleStatus)
				simpleStatus->setText(QString::fromUtf8(u8"LunaHook components not available. Check the LunaHook folder."));
			return;
		}
		lunaEngineActive = true;
		lunaTargetPid = processId;
		if (announce && simpleStatus)
			simpleStatus->setText(QString::fromUtf8(u8"Trying the LunaHook engine (supports more engines)\u2026"));
		// Injeksi bisa memakan waktu -> jalankan di thread terpisah agar UI tidak beku.
		std::thread([processId]
		{
			bool ok = LunaBridge::Instance().ConnectAndInject(processId);
			std::wstring err = LunaBridge::Instance().LastError();
			QMetaObject::invokeMethod(This, [ok, err]
			{
				if (ok)
				{
					if (simpleStatus) simpleStatus->setText(QString::fromUtf8(u8"LunaHook engine active. Open a dialogue in the game."));
					SetHeaderStatus("Connected", true);
					SetEngineBadge(true);
				}
				else
				{
					if (simpleStatus) simpleStatus->setText(QString::fromUtf8(u8"LunaHook injection failed: ") + QString::fromStdWString(err));
					lunaEngineActive = false;
				}
			});
		}).detach();
	}

	// Kembali ke mesin Textractor bawaan: lepas LunaHook, re-attach Textractor.
	void SwitchToTextractor(DWORD processId)
	{
		// Lepas LunaHook dari proses (abaikan error bila belum ter-attach).
		if (LunaBridge::Instance().IsAvailable()) LunaBridge::Instance().Detach(processId);
		Host::RemoveLunaThreads(); // bersihkan thread sintetik LunaHook
		lunaEngineActive = false;
		// reset auto-detect supaya sumber teks Textractor dipilih ulang.
		threadDialogScore.clear();
		autoChosenHandle = 0;
		autoChosenScore = -1;
		lastCurrentTextTick = 0;
		autoPickThread = true;
		sentencesSinceAttach = 0;
		SetEngineBadge(false);
		Host::InjectProcess(processId); // pasang Textractor lagi
		SetHeaderStatus("Connected", true);
		if (simpleStatus) simpleStatus->setText(QString::fromUtf8(u8"Switched back to Textractor. Open a dialogue in the game."));
	}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	This = this;
	ui.setupUi(this);
	extenWindow = new ExtenWindow(this);
	for (auto [text, slot] : Array<const char*, void(&)()>{
		{ ATTACH, AttachProcess },
		{ LAUNCH, LaunchProcess },
		{ CONFIG, ConfigureProcess },
		{ DETACH, DetachProcess },
		{ FORGET, ForgetProcess },
		{ ADD_HOOK, AddHook },
		{ REMOVE_HOOKS, RemoveHooks },
		{ SAVE_HOOKS, SaveHooks },
		{ SEARCH_FOR_HOOKS, FindHooks },
	})
	{
		auto button = new QPushButton(text, ui.processFrame);
		connect(button, &QPushButton::clicked, slot);
		ui.processLayout->addWidget(button);
	}
	// Settings & Extensions dipindah ke panel Settings (tombol "Settings" di header).
	// Sisipkan label kategori di sidebar Advanced agar lebih terorganisir & ringkas.
	{
		// Sisipkan section "GAME" sebelum tombol pertama (Attach) dan "HOOKS" sebelum Add hook.
		auto gameLbl = new QLabel("GAME", ui.processFrame); gameLbl->setObjectName("advSideSection");
		ui.processLayout->insertWidget(1, gameLbl); // setelah processCombo (index 0)
		// Cari indeks tombol "Add hook" untuk menyisipkan label HOOKS sebelumnya.
		for (int i = 0; i < ui.processLayout->count(); ++i)
			if (auto w = qobject_cast<QPushButton*>(ui.processLayout->itemAt(i)->widget()))
				if (w->text() == ADD_HOOK)
				{
					auto hookLbl = new QLabel("HOOKS", ui.processFrame); hookLbl->setObjectName("advSideSection");
					ui.processLayout->insertWidget(i, hookLbl);
					break;
				}
	}
	// Tombol Settings di sidebar Advanced juga (akses cepat ke panel Settings baru).
	{
		auto setBtn = new QPushButton(QString::fromUtf8(u8"\u2699  Settings"), ui.processFrame);
		setBtn->setObjectName("advSidePrimary");
		connect(setBtn, &QPushButton::clicked, [] { OpenSettings(); });
		ui.processLayout->addWidget(setBtn);
	}
	ui.processLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

	// ===================== Bangun Simple Mode UI =====================
	// centralWidget lama (kontrol Textractor) dibungkus jadi "advancedPanel"
	// dengan header modern supaya tidak tampak identik dengan Textractor.
	QWidget* advInner = takeCentralWidget();
	advInner->setObjectName("advInner");

	advancedPanel = new QWidget(this);
	advancedPanel->setObjectName("advRoot");
	auto advLayout = new QVBoxLayout(advancedPanel);
	advLayout->setContentsMargins(0, 0, 0, 0);
	advLayout->setSpacing(0);

	// Header advanced: brand + label mode + tombol kembali ke simpel.
	{
		auto advHeader = new QWidget(advancedPanel);
		advHeader->setObjectName("advHeader");
		auto ahl = new QHBoxLayout(advHeader);
		ahl->setContentsMargins(16, 10, 14, 10);
		ahl->setSpacing(10);
		auto alogo = new QLabel(advHeader);
		{
			QString logoPath = QCoreApplication::applicationDirPath() + "/logo_flower.png";
			QPixmap pm(logoPath);
			if (!pm.isNull()) alogo->setPixmap(pm.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
			else alogo->setText(QString::fromUtf8(u8"\u2740"));
		}
		ahl->addWidget(alogo);
		auto abrand = new QLabel("Shin Translator", advHeader);
		abrand->setObjectName("advBrand");
		ahl->addWidget(abrand);
		auto amode = new QLabel(QString::fromUtf8(u8"Advanced"), advHeader);
		amode->setObjectName("advModeBadge");
		ahl->addWidget(amode);
		ahl->addStretch();
		auto backButton = new QPushButton(QString::fromUtf8(u8"\u2039  Simple Mode"), advHeader);
		backButton->setObjectName("ghostBtn");
		backButton->setCursor(Qt::PointingHandCursor);
		connect(backButton, &QPushButton::clicked, [] { SetSimpleMode(true); });
		ahl->addWidget(backButton);
		advLayout->addWidget(advHeader);
	}
	advLayout->addWidget(advInner, 1);

	auto stack = new QWidget(this);
	stack->setObjectName("stackRoot");
	auto stackLayout = new QVBoxLayout(stack);
	stackLayout->setContentsMargins(0, 0, 0, 0);
	stackLayout->setSpacing(0);

	simplePanel = new QWidget(stack);
	simplePanel->setObjectName("simpleRoot");
	auto rootLayout = new QVBoxLayout(simplePanel);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(0);

	// ---------- HEADER ----------
	auto header = new QWidget(simplePanel);
	header->setObjectName("appHeader");
	auto hl = new QHBoxLayout(header);
	hl->setContentsMargins(18, 12, 16, 12);
	auto logo = new QLabel(header);
	{
		QString logoPath = QCoreApplication::applicationDirPath() + "/logo_flower.png";
		QPixmap pm(logoPath);
		if (!pm.isNull()) logo->setPixmap(pm.scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		else logo->setText(QString::fromUtf8(u8"\u2740"));
	}
	hl->addWidget(logo);
	auto brand = new QLabel("Shin Translator", header);
	brand->setObjectName("brand");
	hl->addWidget(brand);
	hl->addSpacing(12);
	headerStatus = new QLabel(QString::fromUtf8(u8"\u25CB Ready"), header);
	headerStatus->setObjectName("headerStatus");
	hl->addWidget(headerStatus);
	hl->addSpacing(10);
	engineBadge = new QLabel(QString::fromUtf8(u8"Engine: Textractor"), header);
	engineBadge->setObjectName("engineBadge");
	engineBadge->setToolTip(QString::fromUtf8(u8"Active text-hook engine"));
	hl->addWidget(engineBadge);
	hl->addStretch();
	auto settingsBtn = new QPushButton(QString::fromUtf8(u8"\u2699  Settings"), header);
	settingsBtn->setObjectName("ghostBtn");
	settingsBtn->setCursor(Qt::PointingHandCursor);
	connect(settingsBtn, &QPushButton::clicked, [] { OpenSettings(); });
	hl->addWidget(settingsBtn);
	auto advToggle = new QPushButton(QString::fromUtf8(u8"\u2699  Advanced"), header);
	advToggle->setObjectName("ghostBtn");
	advToggle->setCursor(Qt::PointingHandCursor);
	connect(advToggle, &QPushButton::clicked, [] { SetSimpleMode(false); });
	hl->addWidget(advToggle);
	rootLayout->addWidget(header);

	// ---------- BODY (scrollable) ----------
	auto scroll = new QScrollArea(simplePanel);
	scroll->setObjectName("bodyScroll");
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // cegah scroll ke kanan
	auto body = new QWidget();
	body->setObjectName("bodyRoot");
	auto bl = new QVBoxLayout(body);
	bl->setContentsMargins(28, 22, 28, 22);
	bl->setSpacing(10);

	auto h1 = new QLabel("TRANSLATE", body); h1->setObjectName("h1");
	bl->addWidget(h1);
	auto sub = new QLabel(QString::fromUtf8(u8"Translate any visual novel into your language."), body);
	sub->setObjectName("sub");
	bl->addWidget(sub);
	bl->addSpacing(6);

	// Kartu: pilih game
	auto gameCard = new QPushButton(body);
	gameCard->setObjectName("card");
	gameCard->setCursor(Qt::PointingHandCursor);
	auto gcl = new QVBoxLayout(gameCard);
	gcl->setContentsMargins(16, 12, 16, 12);
	gameNameLabel = new QLabel(QString::fromUtf8(u8"\U0001F3AE  Choose a game  \u2192"), gameCard);
	gameNameLabel->setObjectName("cardTitle");
	gcl->addWidget(gameNameLabel);
	auto gcHint = new QLabel(QString::fromUtf8(u8"Click here, then pick the running game/app you want to translate."), gameCard);
	gcHint->setObjectName("cardHint");
	gcHint->setWordWrap(true);
	gcl->addWidget(gcHint);
	connect(gameCard, &QPushButton::clicked, [] { SimplePickGame(); });
	bl->addWidget(gameCard);

	// Kartu: TEXT SOURCE (Auto Detect)
	auto srcLabel = new QLabel("TEXT SOURCE", body); srcLabel->setObjectName("section");
	bl->addWidget(srcLabel);
	auto sourceCard = new QWidget(body); sourceCard->setObjectName("card");
	auto scl = new QVBoxLayout(sourceCard); scl->setContentsMargins(16, 12, 16, 12);
	auto srcTitle = new QLabel(QString::fromUtf8(u8"\u2728 Auto Detect          \u2713"), sourceCard);
	srcTitle->setObjectName("cardTitle");
	scl->addWidget(srcTitle);
	auto srcHint = new QLabel(QString::fromUtf8(u8"Text source recommended automatically"), sourceCard);
	srcHint->setObjectName("cardHint");
	scl->addWidget(srcHint);
	// Baris mesin hook: bawaan (Textractor) + tombol coba LunaHook untuk game yang tak terbaca.
	{
		auto engineRow = new QWidget(sourceCard);
		auto erl = new QHBoxLayout(engineRow); erl->setContentsMargins(0, 6, 0, 0); erl->setSpacing(8);
		auto engineHint = new QLabel(QString::fromUtf8(u8"Game text not showing up?"), engineRow);
		engineHint->setObjectName("cardHint");
		erl->addWidget(engineHint);
		auto lunaBtn = new QPushButton(QString::fromUtf8(u8"\u26A1  Switch to LunaHook"), engineRow);
		lunaBtn->setObjectName("lunaSwitchBtn");
		lunaBtn->setCursor(Qt::PointingHandCursor);
		lunaBtn->setToolTip(QString::fromUtf8(
			u8"Textractor is the default engine. LunaHook is an alternative engine that "
			u8"reads some games Textractor can't. Toggle the engine for this game."));
		engineSwitchBtn = lunaBtn; // simpan agar labelnya bisa berubah (Luna <-> Textractor)
		connect(lunaBtn, &QPushButton::clicked, []
		{
			if (!lunaTargetPid) { SimpleSetStatus(QString::fromUtf8(u8"Select a game first.")); return; }
			if (lunaEngineActive) SwitchToTextractor(lunaTargetPid); // kembali ke Textractor
			else StartLunaEngine(lunaTargetPid, true);                // ke LunaHook
		});
		erl->addWidget(lunaBtn);
		erl->addStretch();
		scl->addWidget(engineRow);
	}
	bl->addWidget(sourceCard);

	// ---------- Kartu OCR (untuk game/menu yang tak bisa di-hook, mis. RPG) ----------
	{
		auto ocrLabel = new QLabel("SCREEN OCR", body); ocrLabel->setObjectName("section");
		bl->addWidget(ocrLabel);
		auto ocrCard = new QWidget(body); ocrCard->setObjectName("card");
		auto ocl = new QVBoxLayout(ocrCard); ocl->setContentsMargins(16, 12, 16, 12); ocl->setSpacing(8);
		auto ocrTitle = new QLabel(QString::fromUtf8(u8"\U0001F4F7  Read text from screen"), ocrCard);
		ocrTitle->setObjectName("cardTitle");
		ocl->addWidget(ocrTitle);
		auto ocrHint = new QLabel(QString::fromUtf8(u8"For games that can't be hooked. Select a screen area to translate."), ocrCard);
		ocrHint->setObjectName("cardHint"); ocrHint->setWordWrap(true);
		ocl->addWidget(ocrHint);

		auto ocrRow = new QWidget(ocrCard);
		auto orl = new QHBoxLayout(ocrRow); orl->setContentsMargins(0, 4, 0, 0); orl->setSpacing(8);
		auto pickBtn = new QPushButton(QString::fromUtf8(u8"\u2702  Select area"), ocrRow);
		pickBtn->setObjectName("lunaSwitchBtn"); pickBtn->setCursor(Qt::PointingHandCursor);
		connect(pickBtn, &QPushButton::clicked, [] { PickOcrRegion(); });
		orl->addWidget(pickBtn);
		ocrNowBtn = new QPushButton(QString::fromUtf8(u8"\U0001F50D  OCR now"), ocrRow);
		ocrNowBtn->setObjectName("miniBtn"); ocrNowBtn->setCursor(Qt::PointingHandCursor);
		ocrNowBtn->setEnabled(false);
		connect(ocrNowBtn, &QPushButton::clicked, [] { RunOcrOnce(); });
		orl->addWidget(ocrNowBtn);
		ocrAutoBtn = new QPushButton(QString::fromUtf8(u8"\u23F1  Auto OCR"), ocrRow);
		ocrAutoBtn->setObjectName("miniBtn"); ocrAutoBtn->setCursor(Qt::PointingHandCursor);
		ocrAutoBtn->setEnabled(false);
		connect(ocrAutoBtn, &QPushButton::clicked, [] { ToggleAutoOcr(); });
		orl->addWidget(ocrAutoBtn);
		// Buka panel riwayat OCR (thumbnail per sesi game) di sisi kanan.
		auto ocrHistBtn = new QPushButton(QString::fromUtf8(u8"\U0001F5BC  OCR History"), ocrRow);
		ocrHistBtn->setObjectName("miniBtn"); ocrHistBtn->setCursor(Qt::PointingHandCursor);
		connect(ocrHistBtn, &QPushButton::clicked, [] { ToggleOcrHistoryPanel(); });
		orl->addWidget(ocrHistBtn);
		orl->addStretch();
		ocl->addWidget(ocrRow);

		ocrStatus = new QLabel(QString::fromUtf8(u8"Windows OCR (offline). Japanese language pack recommended."), ocrCard);
		ocrStatus->setObjectName("cardHint"); ocrStatus->setWordWrap(true);
		ocl->addWidget(ocrStatus);
		bl->addWidget(ocrCard);

		// Muat region OCR tersimpan (bila ada) + aktifkan tombol.
		{
			Settings s;
			int x = s.value("OCR/x", 0).toInt(), y = s.value("OCR/y", 0).toInt();
			int w = s.value("OCR/w", 0).toInt(), h = s.value("OCR/h", 0).toInt();
			if (w > 0 && h > 0) { ocrRegion = QRect(x, y, w, h); ocrNowBtn->setEnabled(true); ocrAutoBtn->setEnabled(true); }
		}
	}

	// Dua kartu preview: Japanese -> Indonesian
	auto previewRow = new QWidget(body);
	auto prl = new QHBoxLayout(previewRow); prl->setContentsMargins(0, 0, 0, 0); prl->setSpacing(12);

	QLabel** badgeOut = nullptr; // opsional: simpan pointer badge (untuk kartu tujuan)
	auto makeTextCard = [&](const QString& badge, const QString& obj) -> QLabel* {
		auto card = new QWidget(previewRow); card->setObjectName("previewCard");
		// Tinggi kartu TETAP -> tidak memanjang mengikuti teks panjang & tidak
		// menyusut acak. Teks yang lebih panjang dari kartu akan bisa di-scroll
		// di dalam kartu (auto-menyesuaikan tanpa mengubah ukuran kartu).
		card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		card->setMinimumHeight(150);
		card->setMaximumHeight(150);
		auto cl = new QVBoxLayout(card); cl->setContentsMargins(14, 12, 14, 14); cl->setSpacing(6);
		auto b = new QLabel(badge, card); b->setObjectName("badge");
		if (badgeOut) *badgeOut = b; // simpan referensi badge bila diminta
		cl->addWidget(b);

		// Area scroll internal: isi teks bebas panjang, kartu tetap.
		auto area = new QScrollArea(card);
		area->setObjectName("previewScroll");
		area->setWidgetResizable(true);
		area->setFrameShape(QFrame::NoFrame);
		area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		area->viewport()->setStyleSheet("background:transparent;");
		area->setStyleSheet("background:transparent;");

		auto txt = new QLabel(QString::fromUtf8(u8"\u2014"));
		txt->setObjectName(obj); txt->setWordWrap(true);
		txt->setTextFormat(Qt::RichText);
		txt->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
		txt->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		// Ignored horizontal: label mengikuti lebar area (tidak memaksa melebar
		// mengikuti teks Jepang panjang tanpa spasi) -> teks selalu wrap ke bawah.
		txt->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
		txt->setMinimumWidth(1);
		area->setWidget(txt);
		cl->addWidget(area, 1);
		prl->addWidget(card, 1);
		return txt;
	};
	// Badge tujuan mengikuti bahasa target terpilih (mis. English), sumber = bahasa asal game.
	auto targetInfo = TargetLanguageBadge();
	srcText = makeTextCard(QString::fromUtf8(u8"\U0001F5E8  Original"), "srcText");
	// Klik kata/kanji di teks asli -> buka side panel kamus.
	connect(srcText, &QLabel::linkActivated, [](const QString& href) { ShowDictionary(href); });
	badgeOut = &dstBadge; // tangkap badge kartu tujuan agar bisa diperbarui live
	dstText = makeTextCard(targetInfo, "dstText");
	badgeOut = nullptr;
	bl->addWidget(previewRow);

	// ---------- Side panel Kamus (Japanese Learning) ----------
	{
		dictPanel = new QWidget(body);
		dictPanel->setObjectName("dictPanel");
		auto dl = new QVBoxLayout(dictPanel); dl->setContentsMargins(16, 12, 16, 14); dl->setSpacing(8);
		auto dhead = new QWidget(dictPanel);
		auto dhl = new QHBoxLayout(dhead); dhl->setContentsMargins(0, 0, 0, 0);
		auto dtitle = new QLabel(QString::fromUtf8(u8"\U0001F4D6  Dictionary"), dhead);
		dtitle->setObjectName("dictHeader");
		dhl->addWidget(dtitle);
		dhl->addStretch();
		auto closeBtn = new QPushButton(QString::fromUtf8(u8"\u2715"), dhead);
		closeBtn->setObjectName("dictClose"); closeBtn->setFixedSize(24, 24); closeBtn->setCursor(Qt::PointingHandCursor);
		connect(closeBtn, &QPushButton::clicked, [] { if (dictPanel) dictPanel->setVisible(false); });
		dhl->addWidget(closeBtn);
		dl->addWidget(dhead);

		dictContent = new QLabel(dictPanel);
		dictContent->setObjectName("dictContent");
		dictContent->setWordWrap(true);
		dictContent->setTextFormat(Qt::RichText);
		dictContent->setTextInteractionFlags(Qt::TextSelectableByMouse);
		dictContent->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		dl->addWidget(dictContent);

		auto saveBtn = new QPushButton(QString::fromUtf8(u8"\u2606  Save to Vocabulary"), dictPanel);
		saveBtn->setObjectName("dictSave"); saveBtn->setCursor(Qt::PointingHandCursor);
		connect(saveBtn, &QPushButton::clicked, [saveBtn]
		{
			if (dictCurrentWord.isEmpty()) return;
			JapaneseLearning::VocabEntry e;
			e.word = dictCurrentWord; e.reading = dictCurrentReading;
			e.meaning = dictCurrentMeaning; e.jlpt = dictCurrentJlpt;
			JapaneseLearning::Instance().AddToVocabulary(e);
			saveBtn->setText(QString::fromUtf8(u8"\u2713  Saved"));
			QTimer::singleShot(1500, saveBtn, [saveBtn] { saveBtn->setText(QString::fromUtf8(u8"\u2606  Save to Vocabulary")); });
		});
		dl->addWidget(saveBtn);
		dictPanel->setVisible(false);
		bl->addWidget(dictPanel);
	}

	// Baris indikator status
	auto statusRow = new QWidget(body);
	auto stl = new QHBoxLayout(statusRow); stl->setContentsMargins(2, 2, 2, 2);
	hookDot = new QLabel(QString::fromUtf8(u8"\u25CF"), statusRow); hookDot->setStyleSheet("color:#c4c4d4;");
	stl->addWidget(hookDot);
	simpleStatus = new QLabel(QString::fromUtf8(u8"Select a game to begin."), statusRow);
	simpleStatus->setObjectName("statusText");
	stl->addWidget(simpleStatus);
	stl->addStretch();
	bl->addWidget(statusRow);

	// Tombol utama
	auto startBtn = new QPushButton(QString::fromUtf8(u8"\u2728  Start Translating"), body);
	startBtn->setObjectName("primaryBtn");
	startBtn->setCursor(Qt::PointingHandCursor);
	connect(startBtn, &QPushButton::clicked, [] { SimplePickGame(); });
	bl->addWidget(startBtn);

	// Tombol "Save this game" (muncul setelah game terpilih).
	saveGameBtn = new QPushButton(QString::fromUtf8(u8"\u2B50  Save this game"), body);
	saveGameBtn->setObjectName("saveGameBtn");
	saveGameBtn->setCursor(Qt::PointingHandCursor);
	saveGameBtn->setToolTip(QString::fromUtf8(u8"Remember this game's engine & hook so it starts translating instantly next time."));
	connect(saveGameBtn, &QPushButton::clicked, [] { SaveCurrentGameProfile(); });
	saveGameBtn->setVisible(false);
	bl->addWidget(saveGameBtn);

	// SAVED GAMES (profil tersimpan: auto pilih Textractor/LunaHook + hook)
	{
		auto savedHeader = new QWidget(body);
		auto shl = new QHBoxLayout(savedHeader); shl->setContentsMargins(0, 6, 0, 0);
		auto savedLabel = new QLabel("SAVED GAMES", savedHeader); savedLabel->setObjectName("section");
		shl->addWidget(savedLabel); shl->addStretch();
		auto importBtn = new QPushButton(QString::fromUtf8(u8"\u2B07  Import"), savedHeader);
		importBtn->setObjectName("miniBtn"); importBtn->setCursor(Qt::PointingHandCursor);
		importBtn->setToolTip(QString::fromUtf8(u8"Import shared game profiles (with hooks) so text appears instantly."));
		connect(importBtn, &QPushButton::clicked, [] { ImportGameProfiles(); });
		shl->addWidget(importBtn);
		auto exportBtn = new QPushButton(QString::fromUtf8(u8"\u2B06  Export"), savedHeader);
		exportBtn->setObjectName("miniBtn"); exportBtn->setCursor(Qt::PointingHandCursor);
		exportBtn->setToolTip(QString::fromUtf8(u8"Export your saved games to share with others."));
		connect(exportBtn, &QPushButton::clicked, [] { ExportGameProfiles(); });
		shl->addWidget(exportBtn);
		bl->addWidget(savedHeader);
	}
	savedGamesList = new QWidget(body);
	{ auto sv = new QVBoxLayout(savedGamesList); sv->setContentsMargins(0, 0, 0, 0); sv->setSpacing(6); }
	bl->addWidget(savedGamesList);

	// HISTORY
	auto histLabel = new QLabel("HISTORY", body); histLabel->setObjectName("section");
	bl->addWidget(histLabel);
	historyList = new QWidget(body);
	historyLayout = new QVBoxLayout(historyList);
	historyLayout->setContentsMargins(0, 0, 0, 0); historyLayout->setSpacing(6);
	bl->addWidget(historyList);
	bl->addStretch();

	scroll->setWidget(body);
	rootLayout->addWidget(scroll, 1);

	stackLayout->addWidget(simplePanel);
	stackLayout->addWidget(advancedPanel);
	setCentralWidget(stack);
	SetSimpleMode(Settings().value(SIMPLE_MODE, true).toBool());

	connect(ui.processCombo, qOverload<int>(&QComboBox::currentIndexChanged), [] { selectedProcessId = ui.processCombo->currentText().split(":")[0].toULong(nullptr, 16); });
	connect(ui.ttCombo, qOverload<int>(&QComboBox::activated), this, ViewThread);
	connect(ui.textOutput, &QPlainTextEdit::selectionChanged, this, CopyUnlessMouseDown);
	connect(ui.textOutput, &QPlainTextEdit::customContextMenuRequested, this, OutputContextMenu);

	Settings settings;
	if (settings.contains(WINDOW) && QApplication::screenAt(settings.value(WINDOW).toRect().center())) setGeometry(settings.value(WINDOW).toRect());
	SetOutputFont(settings.value(FONT, ui.textOutput->font().toString()).toString());
	TextThread::filterRepetition = settings.value(FILTER_REPETITION, TextThread::filterRepetition).toBool();
	autoAttach = settings.value(AUTO_ATTACH, autoAttach).toBool();
	autoAttachSavedOnly = settings.value(ATTACH_SAVED_ONLY, autoAttachSavedOnly).toBool();
	showSystemProcesses = settings.value(SHOW_SYSTEM_PROCESSES, showSystemProcesses).toBool();
	TextThread::flushDelay = settings.value(FLUSH_DELAY, TextThread::flushDelay).toInt();
	TextThread::maxBufferSize = settings.value(MAX_BUFFER_SIZE, TextThread::maxBufferSize).toInt();
	TextThread::maxHistorySize = settings.value(MAX_HISTORY_SIZE, TextThread::maxHistorySize).toInt();
	Host::defaultCodepage = settings.value(DEFAULT_CODEPAGE, Host::defaultCodepage).toInt();

	// Muat data Japanese Learning (offline) di latar agar UI tidak tertunda.
	std::thread([] { JapaneseLearning::Instance().EnsureLoaded(); }).detach();
	RefreshTextProcessingSettings(); // muat setelan pembersihan teks
	GameProfiles::Instance().Load();  // muat profil game tersimpan
	RefreshSavedGamesList();          // tampilkan daftar game tersimpan

	Host::Start(ProcessConnected, ProcessDisconnected, ThreadAdded, ThreadRemoved, SentenceReceived);
	current = &Host::GetThread(Host::console);
	// VN Translator: tidak menampilkan teks 'About' Textractor supaya tampilan bersih.

	// VN Translator: tidak menulis opsi command-line Textractor ke konsol (biar bersih).
	auto processes = GetAllProcesses();
	int argc;
	std::unique_ptr<LPWSTR[], Functor<LocalFree>> argv(CommandLineToArgvW(GetCommandLineW(), &argc));
	for (int i = 0; i < argc; ++i)
		if (std::wstring arg = argv[i]; arg[0] == L'/' || arg[0] == L'-')
		{
			// -luna<pid>: mulai langsung dengan mesin LunaHook untuk game <pid>
			// (dipakai saat versi x64 diluncurkan dari versi x86).
			if (arg.size() > 5 && (arg.compare(1, 4, L"luna") == 0 || arg.compare(1, 4, L"LUNA") == 0))
			{
				if (DWORD processId = wcstoul(arg.substr(5).c_str(), nullptr, 0))
				{
					lunaTargetPid = processId;
					autoPickThread = true;
					if (gameNameLabel) gameNameLabel->setText(QString::fromUtf8(u8"\U0001F3AE  Game (LunaHook)"));
					// Beri jeda agar UI & data siap, lalu aktifkan LunaHook.
					QTimer::singleShot(800, This, [processId] { StartLunaEngine(processId, true); });
				}
			}
			else if (arg[1] == L'p' || arg[1] == L'P')
				if (DWORD processId = wcstoul(arg.substr(2).c_str(), nullptr, 0)) Host::InjectProcess(processId);
				else for (auto [processId, processName] : processes)
					if (processName.value_or(L"").find(L"\\" + arg.substr(2)) != std::string::npos) Host::InjectProcess(processId);
		}

	std::thread([] { for (; ; Sleep(10000)) AttachSavedProcesses(); }).detach();
}

MainWindow::~MainWindow()
{
	Settings().setValue(WINDOW, geometry());
	// Sembunyikan jendela ekstensi (overlay) & proses event agar benar-benar hilang
	// dari layar sebelum DLL dilepas / proses diakhiri.
	for (QWidget* w : QApplication::topLevelWidgets())
		if (w != this) w->hide();
	QApplication::processEvents();
	CleanupExtensions();
	SetErrorMode(SEM_NOGPFAULTERRORBOX);
	ExitProcess(0);
}

void MainWindow::closeEvent(QCloseEvent*)
{
	// Tutup semua jendela tingkat-atas yang dibuat ekstensi (mis. overlay "Extra Window")
	// SEBELUM keluar, agar tidak ada jendela yang tertinggal mengambang di desktop.
	for (QWidget* w : QApplication::topLevelWidgets())
		if (w != this) { w->hide(); w->close(); }
	QApplication::quit(); // Need to do this to kill any windows that might've been made by extensions
}
