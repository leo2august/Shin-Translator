#pragma once

// OCR layar (Windows.Media.Ocr, offline & bawaan Windows). Untuk game/menu yang
// tak bisa di-hook (mis. RPG). Menangkap area layar -> OCR Jepang -> teks.
//
// Implementasi memakai C++/WinRT header-only (tanpa unduhan). API di sini SENGAJA
// tidak mengekspos tipe WinRT supaya file lain (mainwindow) tidak perlu include WinRT.

#include <QString>
#include <QRect>
#include <QImage>
#include <functional>

class OcrEngine
{
public:
	static OcrEngine& Instance();

	// Apakah OCR tersedia + bahasa 'lang' (mis. "ja") didukung di sistem ini.
	bool IsAvailable();
	bool IsLanguageSupported(const QString& langTag);

	// Set bahasa OCR (default "ja"). Mengembalikan false bila tak didukung.
	bool SetLanguage(const QString& langTag);
	QString Language() const { return langTag; }

	// Kenali teks pada area layar (koordinat global). Dijalankan SINKRON (blocking) -
	// pemanggil sebaiknya jalankan di worker thread. Mengembalikan teks (kosong bila gagal).
	QString RecognizeRegion(const QRect& region);

	// Versi asinkron: jalankan di worker thread, panggil callback (di thread pemanggil
	// harus di-marshal sendiri ke GUI). done(teks).
	void RecognizeRegionAsync(const QRect& region, std::function<void(QString)> done);

	// Ambil screenshot area layar sebagai QImage (koordinat global). Kosong bila gagal.
	// Aman dipanggil dari thread mana pun (GDI capture).
	QImage CaptureImage(const QRect& region);

	// Versi asinkron yang mengembalikan TEKS + GAMBAR sekaligus (satu capture untuk keduanya).
	void RecognizeRegionAsync(const QRect& region, std::function<void(QString, QImage)> done);

	QString LastError() const { return lastError; }

private:
	OcrEngine() = default;
	OcrEngine(const OcrEngine&) = delete;
	OcrEngine& operator=(const OcrEngine&) = delete;

	QString langTag = "ja";
	QString lastError;
};
