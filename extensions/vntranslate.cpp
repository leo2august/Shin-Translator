#include "qtcommon.h"
#include "translatewrapper.h"
#include "network.h"

extern const wchar_t* TRANSLATION_ERROR;

// Penerjemah gratis (tanpa API key) untuk VN Translator.
// Urutan usaha: clients5.google.com (dict-chrome-ex) -> translate.google.com/m
// -> api.mymemory.translated.net. Default target: Indonesian.
const char* TRANSLATION_PROVIDER = "VN Translate";
const char* GET_API_KEY_FROM = nullptr; // gratis, tidak butuh kunci

extern const QStringList languagesTo
{
	"Afrikaans", "Albanian", "Amharic", "Arabic", "Armenian", "Azerbaijani",
	"Basque", "Belarusian", "Bengali", "Bosnian", "Bulgarian", "Catalan",
	"Cebuano", "Chichewa", "Chinese (Simplified)", "Chinese (Traditional)",
	"Corsican", "Croatian", "Czech", "Danish", "Dutch", "English", "Esperanto",
	"Estonian", "Filipino", "Finnish", "French", "Frisian", "Galician",
	"Georgian", "German", "Greek", "Gujarati", "Haitian Creole", "Hausa",
	"Hawaiian", "Hebrew", "Hindi", "Hmong", "Hungarian", "Icelandic", "Igbo",
	"Indonesian", "Irish", "Italian", "Japanese", "Javanese", "Kannada",
	"Kazakh", "Khmer", "Kinyarwanda", "Korean", "Kurdish (Kurmanji)", "Kyrgyz",
	"Lao", "Latin", "Latvian", "Lithuanian", "Luxembourgish", "Macedonian",
	"Malagasy", "Malay", "Malayalam", "Maltese", "Maori", "Marathi", "Mongolian",
	"Myanmar (Burmese)", "Nepali", "Norwegian", "Odia (Oriya)", "Pashto",
	"Persian", "Polish", "Portuguese", "Punjabi", "Romanian", "Russian",
	"Samoan", "Scots Gaelic", "Serbian", "Sesotho", "Shona", "Sindhi", "Sinhala",
	"Slovak", "Slovenian", "Somali", "Spanish", "Sundanese", "Swahili",
	"Swedish", "Tajik", "Tamil", "Tatar", "Telugu", "Thai", "Turkish", "Turkmen",
	"Ukrainian", "Urdu", "Uyghur", "Uzbek", "Vietnamese", "Welsh", "Xhosa",
	"Yiddish", "Yoruba", "Zulu",
}, languagesFrom = languagesTo;

extern const std::unordered_map<std::wstring, std::wstring> codes
{
	{ { L"Afrikaans" }, { L"af" } }, { { L"Albanian" }, { L"sq" } },
	{ { L"Amharic" }, { L"am" } }, { { L"Arabic" }, { L"ar" } },
	{ { L"Armenian" }, { L"hy" } }, { { L"Azerbaijani" }, { L"az" } },
	{ { L"Basque" }, { L"eu" } }, { { L"Belarusian" }, { L"be" } },
	{ { L"Bengali" }, { L"bn" } }, { { L"Bosnian" }, { L"bs" } },
	{ { L"Bulgarian" }, { L"bg" } }, { { L"Catalan" }, { L"ca" } },
	{ { L"Cebuano" }, { L"ceb" } }, { { L"Chichewa" }, { L"ny" } },
	{ { L"Chinese (Simplified)" }, { L"zh-CN" } },
	{ { L"Chinese (Traditional)" }, { L"zh-TW" } },
	{ { L"Corsican" }, { L"co" } }, { { L"Croatian" }, { L"hr" } },
	{ { L"Czech" }, { L"cs" } }, { { L"Danish" }, { L"da" } },
	{ { L"Dutch" }, { L"nl" } }, { { L"English" }, { L"en" } },
	{ { L"Esperanto" }, { L"eo" } }, { { L"Estonian" }, { L"et" } },
	{ { L"Filipino" }, { L"tl" } }, { { L"Finnish" }, { L"fi" } },
	{ { L"French" }, { L"fr" } }, { { L"Frisian" }, { L"fy" } },
	{ { L"Galician" }, { L"gl" } }, { { L"Georgian" }, { L"ka" } },
	{ { L"German" }, { L"de" } }, { { L"Greek" }, { L"el" } },
	{ { L"Gujarati" }, { L"gu" } }, { { L"Haitian Creole" }, { L"ht" } },
	{ { L"Hausa" }, { L"ha" } }, { { L"Hawaiian" }, { L"haw" } },
	{ { L"Hebrew" }, { L"iw" } }, { { L"Hindi" }, { L"hi" } },
	{ { L"Hmong" }, { L"hmn" } }, { { L"Hungarian" }, { L"hu" } },
	{ { L"Icelandic" }, { L"is" } }, { { L"Igbo" }, { L"ig" } },
	{ { L"Indonesian" }, { L"id" } }, { { L"Irish" }, { L"ga" } },
	{ { L"Italian" }, { L"it" } }, { { L"Japanese" }, { L"ja" } },
	{ { L"Javanese" }, { L"jw" } }, { { L"Kannada" }, { L"kn" } },
	{ { L"Kazakh" }, { L"kk" } }, { { L"Khmer" }, { L"km" } },
	{ { L"Kinyarwanda" }, { L"rw" } }, { { L"Korean" }, { L"ko" } },
	{ { L"Kurdish (Kurmanji)" }, { L"ku" } }, { { L"Kyrgyz" }, { L"ky" } },
	{ { L"Lao" }, { L"lo" } }, { { L"Latin" }, { L"la" } },
	{ { L"Latvian" }, { L"lv" } }, { { L"Lithuanian" }, { L"lt" } },
	{ { L"Luxembourgish" }, { L"lb" } }, { { L"Macedonian" }, { L"mk" } },
	{ { L"Malagasy" }, { L"mg" } }, { { L"Malay" }, { L"ms" } },
	{ { L"Malayalam" }, { L"ml" } }, { { L"Maltese" }, { L"mt" } },
	{ { L"Maori" }, { L"mi" } }, { { L"Marathi" }, { L"mr" } },
	{ { L"Mongolian" }, { L"mn" } }, { { L"Myanmar (Burmese)" }, { L"my" } },
	{ { L"Nepali" }, { L"ne" } }, { { L"Norwegian" }, { L"no" } },
	{ { L"Odia (Oriya)" }, { L"or" } }, { { L"Pashto" }, { L"ps" } },
	{ { L"Persian" }, { L"fa" } }, { { L"Polish" }, { L"pl" } },
	{ { L"Portuguese" }, { L"pt" } }, { { L"Punjabi" }, { L"pa" } },
	{ { L"Romanian" }, { L"ro" } }, { { L"Russian" }, { L"ru" } },
	{ { L"Samoan" }, { L"sm" } }, { { L"Scots Gaelic" }, { L"gd" } },
	{ { L"Serbian" }, { L"sr" } }, { { L"Sesotho" }, { L"st" } },
	{ { L"Shona" }, { L"sn" } }, { { L"Sindhi" }, { L"sd" } },
	{ { L"Sinhala" }, { L"si" } }, { { L"Slovak" }, { L"sk" } },
	{ { L"Slovenian" }, { L"sl" } }, { { L"Somali" }, { L"so" } },
	{ { L"Spanish" }, { L"es" } }, { { L"Sundanese" }, { L"su" } },
	{ { L"Swahili" }, { L"sw" } }, { { L"Swedish" }, { L"sv" } },
	{ { L"Tajik" }, { L"tg" } }, { { L"Tamil" }, { L"ta" } },
	{ { L"Tatar" }, { L"tt" } }, { { L"Telugu" }, { L"te" } },
	{ { L"Thai" }, { L"th" } }, { { L"Turkish" }, { L"tr" } },
	{ { L"Turkmen" }, { L"tk" } }, { { L"Ukrainian" }, { L"uk" } },
	{ { L"Urdu" }, { L"ur" } }, { { L"Uyghur" }, { L"ug" } },
	{ { L"Uzbek" }, { L"uz" } }, { { L"Vietnamese" }, { L"vi" } },
	{ { L"Welsh" }, { L"cy" } }, { { L"Xhosa" }, { L"xh" } },
	{ { L"Yiddish" }, { L"yi" } }, { { L"Yoruba" }, { L"yo" } },
	{ { L"Zulu" }, { L"zu" } }, { { L"?" }, { L"auto" } }
};

bool translateSelectedOnly = false, useRateLimiter = true, rateLimitSelected = false, useCache = true, useFilter = true;
// tokenCount dinaikkan 30 -> 90: adegan cepat (mis. eroge) mengirim banyak baris pendek
// dalam waktu singkat; 30/60dtk terlalu ketat sehingga banyak baris ditolak
// ("Rate limit exceeded"). Request GET singkat, jadi 90/60dtk masih aman dari blokir.
int tokenCount = 90, rateLimitTimespan = 60000, maxSentenceSize = 1000;

namespace
{
	// Ambil isi string JSON pertama dari respons clients5 dict-chrome-ex.
	// Format khas: [["terjemahan","asli"], ...] atau {"sentences":[{"trans":"..."}]}
	std::optional<std::wstring> ParseClients5(const std::wstring& response)
	{
		auto json = JSON::Parse(response);
		// Bentuk objek {"sentences":[{"trans":"..."}]}
		if (auto sentences = json[L"sentences"].Array())
		{
			std::wstring joined;
			for (const auto& s : *sentences)
				if (auto trans = s[L"trans"].String()) joined += *trans;
			if (!joined.empty()) return joined;
		}
		// Bentuk array [["...", "..."], ...] atau ["..."]
		if (auto arr = json.Array())
		{
			std::wstring joined;
			for (const auto& seg : *arr)
			{
				if (auto str = seg.String()) joined += *str;
				else if (auto inner = seg.Array())
					if (!inner->empty()) if (auto str = inner->at(0).String()) joined += *str;
			}
			if (!joined.empty()) return joined;
		}
		return std::nullopt;
	}

	// Parser untuk endpoint translate_a/single (client=gtx, dt=t).
	// Format: [[["terjemahan","asli",...],["...","..."]], ...]
	// Terjemahan tiap kalimat ada di array[0][i][0]; gabungkan jadi kalimat penuh.
	std::optional<std::wstring> ParseSingle(const std::wstring& response)
	{
		auto json = JSON::Parse(response);
		if (auto outer = json.Array())
			if (!outer->empty())
				if (auto sentences = outer->at(0).Array())
				{
					std::wstring joined;
					for (const auto& s : *sentences)
						if (auto seg = s.Array())
							if (!seg->empty())
								if (auto str = seg->at(0).String()) joined += *str;
					if (!joined.empty()) return joined;
				}
		return std::nullopt;
	}
}

std::pair<bool, std::wstring> Translate(const std::wstring& text, TranslationParam tlp)
{
	std::wstring sl = tlp.translateFrom == L"?" ? L"auto" : codes.at(tlp.translateFrom);
	std::wstring tl = codes.at(tlp.translateTo);
	std::wstring lastError;

	// 1) clients5.google.com (endpoint kamus Chrome) - dijadikan UTAMA karena cepat &
	//    JARANG kena rate-limit (429). translate.googleapis.com sering balas 429 pada
	//    IP yang sama -> bikin fallback beruntun & lambat. Endpoint ini stabil.
	if (HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		L"clients5.google.com",
		L"GET",
		FormatString(L"/translate_a/t?client=dict-chrome-ex&sl=%s&tl=%s&q=%s",
			sl, tl, Escape(text)).c_str()
	})
	{
		if (auto translation = ParseClients5(httpRequest.response))
			return { true, translation.value() };
		lastError = FormatString(L"%s: %s", TRANSLATION_ERROR, httpRequest.response);
	}
	else lastError = FormatString(L"%s (code=%u)", TRANSLATION_ERROR, httpRequest.errorCode);

	// 2) translate.googleapis.com/translate_a/single (gtx) - kalimat penuh, cadangan.
	if (HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		L"translate.googleapis.com",
		L"GET",
		FormatString(L"/translate_a/single?client=gtx&sl=%s&tl=%s&dt=t&q=%s",
			sl, tl, Escape(text)).c_str()
	})
	{
		if (auto translation = ParseSingle(httpRequest.response))
			return { true, translation.value() };
	}

	// 3) translate.google.com/m (cadangan, parse blok result-container)
	if (HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		L"translate.google.com",
		L"GET",
		FormatString(L"/m?sl=%s&tl=%s&q=%s", sl, tl, Escape(text)).c_str()
	})
	{
		auto start = httpRequest.response.find(L"result-container\">");
		if (start != std::wstring::npos)
		{
			auto end = httpRequest.response.find(L'<', start);
			if (end != std::wstring::npos)
				return { true, HTML::Unescape(httpRequest.response.substr(start + 18, end - start - 18)) };
		}
	}

	// 3) MyMemory (cadangan terakhir; sl 'auto' tidak didukung -> pakai ja bila auto)
	std::wstring myMemFrom = (sl == L"auto") ? L"ja" : sl;
	if (HttpRequest httpRequest{
		L"Mozilla/5.0 Textractor",
		L"api.mymemory.translated.net",
		L"GET",
		FormatString(L"/get?q=%s&langpair=%s|%s", Escape(text), myMemFrom, tl).c_str()
	})
	{
		if (auto translated = Copy(JSON::Parse(httpRequest.response)
				[L"responseData"][L"translatedText"].String()))
			if (!translated->empty()) return { true, HTML::Unescape(translated.value()) };
	}

	return { false, lastError.empty() ? std::wstring(TRANSLATION_ERROR) : lastError };
}
