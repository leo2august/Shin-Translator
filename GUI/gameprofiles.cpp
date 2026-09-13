#include "gameprofiles.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

GameProfiles& GameProfiles::Instance()
{
	static GameProfiles instance;
	return instance;
}

QString GameProfiles::FilePath() const
{
	return QCoreApplication::applicationDirPath() + "/game_profiles.json";
}

void GameProfiles::Load()
{
	loaded = true;
	profiles.clear();
	QFile f(FilePath());
	if (!f.open(QIODevice::ReadOnly)) return;
	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	f.close();
	if (!doc.isArray()) return;
	for (const auto& v : doc.array())
	{
		QJsonObject o = v.toObject();
		Profile p;
		p.exe = o.value("exe").toString();
		p.title = o.value("title").toString();
		p.engine = o.value("engine").toString("textractor");
		p.targetLang = o.value("targetLang").toString();
		p.hookCode = o.value("hookCode").toString();
		if (p.title.isEmpty()) p.title = p.exe;
		if (!p.exe.isEmpty()) profiles.push_back(p);
	}
}

void GameProfiles::Save() const
{
	QJsonArray arr;
	for (const auto& p : profiles)
	{
		QJsonObject o;
		o.insert("exe", p.exe);
		o.insert("title", p.title);
		o.insert("engine", p.engine);
		o.insert("targetLang", p.targetLang);
		o.insert("hookCode", p.hookCode);
		arr.append(o);
	}
	QFile f(FilePath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
		f.close();
	}
}

const GameProfiles::Profile* GameProfiles::Find(const QString& exe) const
{
	for (const auto& p : profiles)
		if (p.exe.compare(exe, Qt::CaseInsensitive) == 0) return &p;
	return nullptr;
}

void GameProfiles::Set(const Profile& p)
{
	if (!loaded) Load();
	for (auto& e : profiles)
		if (e.exe.compare(p.exe, Qt::CaseInsensitive) == 0)
		{
			QString keepTitle = e.title.isEmpty() ? p.title : e.title;
			e = p;
			if (!keepTitle.isEmpty() && keepTitle != p.exe) e.title = keepTitle; // pertahankan judul edit
			Save();
			return;
		}
	Profile np = p;
	if (np.title.isEmpty()) np.title = np.exe;
	profiles.push_back(np);
	Save();
}

void GameProfiles::Remove(const QString& exe)
{
	for (int i = 0; i < profiles.size(); ++i)
		if (profiles[i].exe.compare(exe, Qt::CaseInsensitive) == 0) { profiles.remove(i); Save(); return; }
}

void GameProfiles::Rename(const QString& exe, const QString& newTitle)
{
	for (auto& p : profiles)
		if (p.exe.compare(exe, Qt::CaseInsensitive) == 0) { p.title = newTitle; Save(); return; }
}

bool GameProfiles::ExportToFile(const QString& path) const
{
	QJsonObject root;
	root.insert("format", "vn-translator-profiles");
	root.insert("version", 1);
	QJsonArray arr;
	for (const auto& p : profiles)
	{
		QJsonObject o;
		o.insert("exe", p.exe);
		o.insert("title", p.title);
		o.insert("engine", p.engine);
		o.insert("targetLang", p.targetLang);
		o.insert("hookCode", p.hookCode);
		arr.append(o);
	}
	root.insert("profiles", arr);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
	f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	f.close();
	return true;
}

int GameProfiles::ImportFromFile(const QString& path)
{
	if (!loaded) Load();
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return -1;
	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	f.close();
	// Terima dua bentuk: {format,profiles:[...]} ATAU array langsung.
	QJsonArray arr;
	if (doc.isObject()) arr = doc.object().value("profiles").toArray();
	else if (doc.isArray()) arr = doc.array();
	else return -1;
	int count = 0;
	for (const auto& v : arr)
	{
		QJsonObject o = v.toObject();
		Profile p;
		p.exe = o.value("exe").toString();
		p.title = o.value("title").toString();
		p.engine = o.value("engine").toString("textractor");
		p.targetLang = o.value("targetLang").toString();
		p.hookCode = o.value("hookCode").toString();
		if (p.title.isEmpty()) p.title = p.exe;
		if (p.exe.isEmpty()) continue;
		Set(p); // upsert (Set memanggil Save)
		count++;
	}
	return count;
}
