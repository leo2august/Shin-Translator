#pragma once

// Japanese Learning (Fase 2, Opsi A - offline, ringan).
// Memuat data kanji + kosakata JLPT dari <exe>\jp_data\ dan menyediakan
// pencarian untuk fitur belajar: arti kanji, level JLPT, bacaan (furigana),
// serta Vocabulary / Word Bank yang tersimpan ke <exe>\vocabulary.json.
//
// Data (offline, disertakan di paket):
//   jp_data\kanji.json      - per-kanji: meanings, jlpt, readings on/kun, strokes
//   jp_data\jlpt_vocab.json - per-kata : reading (kana) + level JLPT
//
// Tanpa tokenizer morfologi (lihat catatan riset), sehingga furigana hanya
// tersedia untuk kata yang cocok penuh di kamus; klik-kamus bekerja per-kanji
// dan per-kata (pencocokan maju terpanjang sederhana).

#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
#include <QChar>

class JapaneseLearning
{
public:
	struct KanjiInfo
	{
		QChar kanji;
		QStringList meanings;
		QStringList onReadings;
		QStringList kunReadings;
		int jlpt = 0;   // 5=N5 (mudah) .. 1=N1 (sulit); 0=tak diketahui
		int strokes = 0;
		int grade = 0;
		bool found = false;
	};

	struct WordInfo
	{
		QString word;
		QString reading; // kana (untuk furigana)
		int jlpt = 0;    // 5..1, 0=tak diketahui
		bool found = false;
	};

	static JapaneseLearning& Instance();

	// Muat data JSON sekali (idempoten). Mengembalikan false bila file tak ada.
	bool EnsureLoaded();
	bool IsAvailable() const { return loaded; }

	KanjiInfo LookupKanji(QChar c) const;
	WordInfo LookupWord(const QString& word) const; // exact match di kamus JLPT

	// Pencocokan maju terpanjang mulai dari indeks tertentu (tanpa tokenizer):
	// kembalikan panjang kata terpanjang (>=1) yang ada di kamus mulai di 'start'.
	// Berguna untuk klik-kamus & furigana per-segmen.
	int MatchWordLengthAt(const QString& text, int start, int maxLen = 8) const;

	static QString JlptLabel(int level); // 5 -> "N5", 0 -> ""
	static bool IsKanji(QChar c);
	static bool IsKana(QChar c);

	// ---- Vocabulary / Word Bank ----
	struct VocabEntry { QString word; QString reading; QString meaning; int jlpt = 0; };
	const QVector<VocabEntry>& Vocabulary() const { return vocab; }
	bool IsSaved(const QString& word) const;
	void AddToVocabulary(const VocabEntry& entry);   // simpan + tulis file
	void RemoveFromVocabulary(const QString& word);
	void LoadVocabulary();
	void SaveVocabulary() const;

private:
	JapaneseLearning() = default;
	JapaneseLearning(const JapaneseLearning&) = delete;
	JapaneseLearning& operator=(const JapaneseLearning&) = delete;

	QString DataDir() const;   // <exe>\jp_data
	QString VocabFile() const; // <exe>\vocabulary.json

	bool loaded = false;

	// kanji -> info
	QHash<QChar, KanjiInfo> kanjiMap;
	// word -> {reading, jlpt}
	QHash<QString, WordInfo> wordMap;

	QVector<VocabEntry> vocab;
};
