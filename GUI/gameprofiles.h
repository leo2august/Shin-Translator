#pragma once

// Game Profiles: simpan preferensi per-game (mesin hook Textractor/LunaHook + bahasa target)
// agar saat game dibuka lagi, VN Translator langsung memakai mesin yang tepat tanpa
// deteksi ulang. Hook Textractor sendiri sudah disimpan lewat SavedHooks.txt (mekanisme
// bawaan); modul ini melengkapi dengan pilihan ENGINE + daftar yang bisa diedit/hapus.
//
// Disimpan ke <exe>\game_profiles.json.

#include <QString>
#include <QVector>

class GameProfiles
{
public:
	struct Profile
	{
		QString exe;        // nama file exe game (mis. "game.exe") - kunci pencocokan
		QString title;      // nama tampilan (default = exe, bisa diedit user)
		QString engine;     // "textractor" atau "luna"
		QString targetLang; // bahasa target saat disimpan (opsional)
		QString hookCode;   // kode hook Textractor (agar teks langsung muncul saat di-share)
	};

	static GameProfiles& Instance();

	void Load();
	void Save() const;

	const QVector<Profile>& All() const { return profiles; }

	// Cari profil berdasarkan nama exe (case-insensitive). nullptr bila tak ada.
	const Profile* Find(const QString& exe) const;

	// Tambah/perbarui profil untuk exe (upsert).
	void Set(const Profile& p);
	void Remove(const QString& exe);
	void Rename(const QString& exe, const QString& newTitle);

	// Import/Export untuk berbagi profil (termasuk hook code) antar pengguna.
	// Export: tulis semua profil ke file JSON. Import: gabungkan dari file (upsert).
	bool ExportToFile(const QString& path) const;
	int ImportFromFile(const QString& path); // kembalikan jumlah profil yang diimpor (-1 = gagal)

private:
	GameProfiles() = default;
	GameProfiles(const GameProfiles&) = delete;
	GameProfiles& operator=(const GameProfiles&) = delete;
	QString FilePath() const;
	QVector<Profile> profiles;
	bool loaded = false;
};
