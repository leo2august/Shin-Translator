#include "ocrengine.h"

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <robuffer.h> // IBufferByteAccess
#include <thread>
#include <vector>

// Hindari `using namespace ...::Ocr` karena bentrok dgn kelas OcrEngine milik kita.
namespace WMO = winrt::Windows::Media::Ocr;
namespace WG = winrt::Windows::Globalization; // WG::Language (hindari bentrok method Language() kita)
using winrt::Windows::Graphics::Imaging::SoftwareBitmap;
using winrt::Windows::Graphics::Imaging::BitmapPixelFormat;
using winrt::Windows::Graphics::Imaging::BitmapAlphaMode;
using winrt::Windows::Storage::Streams::Buffer;

OcrEngine& OcrEngine::Instance()
{
	static OcrEngine instance;
	return instance;
}

namespace
{
	// Pastikan apartment WinRT terinisialisasi di thread saat ini (idempoten aman).
	void EnsureApartment()
	{
		static thread_local bool inited = false;
		if (!inited)
		{
			try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
			catch (...) {}
			inited = true;
		}
	}

	// Tangkap area layar (koordinat global) -> data BGRA (top-down) + ukuran.
	bool CaptureRegion(const QRect& r, std::vector<BYTE>& outBgra, int& w, int& h)
	{
		w = r.width(); h = r.height();
		if (w <= 0 || h <= 0) return false;

		HDC screenDC = GetDC(nullptr);
		if (!screenDC) return false;
		HDC memDC = CreateCompatibleDC(screenDC);

		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h; // negatif -> top-down (baris pertama = atas)
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		void* bits = nullptr;
		HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
		bool ok = false;
		if (dib && bits)
		{
			HGDIOBJ old = SelectObject(memDC, dib);
			// Salin dari layar (termasuk layer window lain) ke DIB.
			ok = BitBlt(memDC, 0, 0, w, h, screenDC, r.x(), r.y(), SRCCOPY | CAPTUREBLT) != 0;
			if (ok)
			{
				outBgra.resize((size_t)w * h * 4);
				memcpy(outBgra.data(), bits, outBgra.size());
				// DIB 32bpp: byte order = B,G,R,A(unused). Set alpha=255 agar opaque.
				for (size_t i = 3; i < outBgra.size(); i += 4) outBgra[i] = 255;
			}
			SelectObject(memDC, old);
		}
		if (dib) DeleteObject(dib);
		DeleteDC(memDC);
		ReleaseDC(nullptr, screenDC);
		return ok;
	}

	// Buat SoftwareBitmap Bgra8 dari buffer BGRA.
	SoftwareBitmap MakeBitmap(const std::vector<BYTE>& bgra, int w, int h)
	{
		SoftwareBitmap bmp(BitmapPixelFormat::Bgra8, w, h, BitmapAlphaMode::Premultiplied);
		Buffer buffer((uint32_t)bgra.size());
		buffer.Length((uint32_t)bgra.size());
		// Akses byte mentah IBuffer.
		auto byteAccess = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
		BYTE* dst = nullptr;
		byteAccess->Buffer(&dst);
		if (dst) memcpy(dst, bgra.data(), bgra.size());
		bmp.CopyFromBuffer(buffer);
		return bmp;
	}
}

bool OcrEngine::IsAvailable()
{
	EnsureApartment();
	try
	{
		auto langs = winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages();
		return langs.Size() > 0;
	}
	catch (...) { return false; }
}

bool OcrEngine::IsLanguageSupported(const QString& tag)
{
	EnsureApartment();
	try { return winrt::Windows::Media::Ocr::OcrEngine::IsLanguageSupported(WG::Language(winrt::hstring(tag.toStdWString()))); }
	catch (...) { return false; }
}

bool OcrEngine::SetLanguage(const QString& tag)
{
	if (IsLanguageSupported(tag)) { langTag = tag; return true; }
	return false;
}

namespace
{
	// OCR pada buffer BGRA yang sudah ditangkap. Set errOut bila gagal.
	QString RecognizeBgra(const std::vector<BYTE>& bgra, int w, int h, const QString& langTag, QString& errOut)
	{
		try
		{
			auto engine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(WG::Language(winrt::hstring(langTag.toStdWString())));
			if (!engine) engine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
			if (!engine) { errOut = "OCR engine unavailable (install the language pack)"; return {}; }
			auto bmp = MakeBitmap(bgra, w, h);
			auto result = engine.RecognizeAsync(bmp).get();
			return QString::fromWCharArray(result.Text().c_str());
		}
		catch (winrt::hresult_error const& e) { errOut = QString::fromWCharArray(e.message().c_str()); return {}; }
		catch (...) { errOut = "OCR failed"; return {}; }
	}

	// Konversi buffer BGRA (top-down, alpha=255) -> QImage (deep copy, aman lintas thread).
	QImage BgraToImage(const std::vector<BYTE>& bgra, int w, int h)
	{
		if (w <= 0 || h <= 0 || bgra.size() < (size_t)w * h * 4) return QImage();
		// QImage::Format_ARGB32 di little-endian = byte order B,G,R,A -> cocok dgn BGRA kita.
		QImage img(reinterpret_cast<const uchar*>(bgra.data()), w, h, w * 4, QImage::Format_ARGB32);
		return img.copy(); // deep copy: lepas dari buffer sementara
	}
}

QString OcrEngine::RecognizeRegion(const QRect& region)
{
	EnsureApartment();
	lastError.clear();
	std::vector<BYTE> bgra; int w = 0, h = 0;
	if (!CaptureRegion(region, bgra, w, h)) { lastError = "Screen capture failed"; return {}; }
	QString err;
	QString text = RecognizeBgra(bgra, w, h, langTag, err);
	if (!err.isEmpty()) lastError = err;
	return text;
}

QImage OcrEngine::CaptureImage(const QRect& region)
{
	std::vector<BYTE> bgra; int w = 0, h = 0;
	if (!CaptureRegion(region, bgra, w, h)) return QImage();
	return BgraToImage(bgra, w, h);
}

void OcrEngine::RecognizeRegionAsync(const QRect& region, std::function<void(QString)> done)
{
	std::thread([region, done]
	{
		QString text = OcrEngine::Instance().RecognizeRegion(region);
		if (done) done(text);
	}).detach();
}

void OcrEngine::RecognizeRegionAsync(const QRect& region, std::function<void(QString, QImage)> done)
{
	std::thread([region, done]
	{
		EnsureApartment();
		OcrEngine& self = OcrEngine::Instance();
		self.lastError.clear();
		std::vector<BYTE> bgra; int w = 0, h = 0;
		QImage img; QString text;
		if (!CaptureRegion(region, bgra, w, h)) { self.lastError = "Screen capture failed"; }
		else
		{
			img = BgraToImage(bgra, w, h);
			QString err;
			text = RecognizeBgra(bgra, w, h, self.langTag, err);
			if (!err.isEmpty()) self.lastError = err;
		}
		if (done) done(text, img);
	}).detach();
}
