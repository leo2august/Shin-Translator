#include "japaneselearning.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

JapaneseLearning& JapaneseLearning::Instance()
{
	static JapaneseLearning instance;
	return instance;
}

QString JapaneseLearning::DataDir() const
{
	return QCoreApplication::applicationDirPath() + "/jp_data";
}

QString JapaneseLearning::VocabFile() const
{
	return QCoreApplication::applicationDirPath() + "/vocabulary.json";
}

bool JapaneseLearning::IsKanji(QChar c)
{
	ushort u = c.unicode();
	return (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF);
}

bool JapaneseLearning::IsKana(QChar c)
{
	ushort u = c.unicode();
	return (u >= 0x3040 && u <= 0x309F) || (u >= 0x30A0 && u <= 0x30FF);
}

QString JapaneseLearning::JlptLabel(int level)
{
	if (level >= 1 && level <= 5) return QString("N%1").arg(level);
	return QString();
}

static QStringList ToStringList(const QJsonValue& v)
{
	QStringList out;
	if (v.isArray()) for (const auto& e : v.toArray()) out << e.toString();
	return out;
}

bool JapaneseLearning::EnsureLoaded()
{
	if (loaded) return true;

	// --- kanji.json ---
	QFile kf(DataDir() + "/kanji.json");
	if (!kf.open(QIODevice::ReadOnly)) return false;
	QJsonParseError err{};
	QJsonDocument kdoc = QJsonDocument::fromJson(kf.readAll(), &err);
	kf.close();
	if (err.error != QJsonParseError::NoError || !kdoc.isObject()) return false;

	QJsonObject kobj = kdoc.object();
	for (auto it = kobj.begin(); it != kobj.end(); ++it)
	{
		QString key = it.key();
		if (key.isEmpty()) continue;
		QChar c = key.at(0);
		QJsonObject e = it.value().toObject();
		KanjiInfo info;
		info.kanji = c;
		info.meanings = ToStringList(e.value("meanings"));
		info.onReadings = ToStringList(e.value("readings_on"));
		info.kunReadings = ToStringList(e.value("readings_kun"));
		info.jlpt = e.value("jlpt_new").toInt(0);
		info.strokes = e.value("strokes").toInt(0);
		info.grade = e.value("grade").toInt(0);
		info.found = true;
		kanjiMap.insert(c, info);
	}

	// --- jlpt_vocab.json ---  { "word": [ { "reading": "..", "level": N } ] }
	QFile vf(DataDir() + "/jlpt_vocab.json");
	if (vf.open(QIODevice::ReadOnly))
	{
		QJsonDocument vdoc = QJsonDocument::fromJson(vf.readAll());
		vf.close();
		if (vdoc.isObject())
		{
			QJsonObject vobj = vdoc.object();
			for (auto it = vobj.begin(); it != vobj.end(); ++it)
			{
				QJsonArray arr = it.value().toArray();
				if (arr.isEmpty()) continue;
				QJsonObject first = arr.first().toObject();
				WordInfo wi;
				wi.word = it.key();
				wi.reading = first.value("reading").toString();
				wi.jlpt = first.value("level").toInt(0);
				wi.found = true;
				wordMap.insert(wi.word, wi);
			}
		}
	}

	loaded = !kanjiMap.isEmpty();
	if (loaded) LoadVocabulary();
	return loaded;
}

JapaneseLearning::KanjiInfo JapaneseLearning::LookupKanji(QChar c) const
{
	auto it = kanjiMap.find(c);
	if (it != kanjiMap.end()) return it.value();
	KanjiInfo none; none.kanji = c; return none;
}

JapaneseLearning::WordInfo JapaneseLearning::LookupWord(const QString& word) const
{
	auto it = wordMap.find(word);
	if (it != wordMap.end()) return it.value();
	WordInfo none; none.word = word; return none;
}

int JapaneseLearning::MatchWordLengthAt(const QString& text, int start, int maxLen) const
{
	int n = text.size();
	int limit = qMin(maxLen, n - start);
	for (int len = limit; len >= 2; --len)
	{
		if (wordMap.contains(text.mid(start, len))) return len;
	}
	return 1; // minimal satu karakter (untuk klik per-kanji)
}

// ---- Vocabulary ----

bool JapaneseLearning::IsSaved(const QString& word) const
{
	for (const auto& e : vocab) if (e.word == word) return true;
	return false;
}

void JapaneseLearning::AddToVocabulary(const VocabEntry& entry)
{
	if (entry.word.isEmpty() || IsSaved(entry.word)) return;
	vocab.push_back(entry);
	SaveVocabulary();
}

void JapaneseLearning::RemoveFromVocabulary(const QString& word)
{
	for (int i = 0; i < vocab.size(); ++i)
		if (vocab[i].word == word) { vocab.remove(i); SaveVocabulary(); return; }
}

void JapaneseLearning::LoadVocabulary()
{
	vocab.clear();
	QFile f(VocabFile());
	if (!f.open(QIODevice::ReadOnly)) return;
	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	f.close();
	if (!doc.isArray()) return;
	for (const auto& v : doc.array())
	{
		QJsonObject o = v.toObject();
		VocabEntry e;
		e.word = o.value("word").toString();
		e.reading = o.value("reading").toString();
		e.meaning = o.value("meaning").toString();
		e.jlpt = o.value("jlpt").toInt(0);
		if (!e.word.isEmpty()) vocab.push_back(e);
	}
}

void JapaneseLearning::SaveVocabulary() const
{
	QJsonArray arr;
	for (const auto& e : vocab)
	{
		QJsonObject o;
		o.insert("word", e.word);
		o.insert("reading", e.reading);
		o.insert("meaning", e.meaning);
		o.insert("jlpt", e.jlpt);
		arr.append(o);
	}
	QFile f(VocabFile());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
		f.close();
	}
}
