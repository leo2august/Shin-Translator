#pragma once

// LunaBridge: jembatan Opsi B ke LunaHook (dari rilis LunaTranslator).
// Memuat LunaHost64.dll secara dinamis (LoadLibrary + GetProcAddress),
// menangani injeksi lewat LunaSubProcess<bit>.exe (bukan tulis ulang injeksi),
// lalu meneruskan teks hook (OutputCallback) ke pipeline VN Translator.
//
// Komponen yang diharapkan berada di:  <folder exe>\LunaHook\
//   LunaHost64.dll, LunaHook64.dll, LunaHook32.dll
// dan LunaSubProcess64.exe / LunaSubProcess32.exe di folder yang sama.

#include <cstdint>
#include <string>
#include <functional>
#include <windows.h>

class LunaBridge
{
public:
	// ThreadParam LunaHook (sama persis dgn LunaTranslator texthook.py).
	struct ThreadParam { uint32_t processId; uint64_t addr; uint64_t ctx; uint64_t ctx2; };

	// Dipanggil saat teks baru datang dari game (thread UI aman: pemanggil harus
	// men-marshal ke thread Qt; MainWindow memakai QMetaObject::invokeMethod).
	// hookName = nama hook (mis. "HB8@...:GameEngine"), text = teks mentah (UTF-16).
	using OutputHandler = std::function<void(uint64_t ctx, uint64_t ctx2,
		const std::wstring& hookName, const std::wstring& text)>;
	// Diinformasikan saat hook baru terdeteksi (untuk daftar hook manual / status).
	using HookHandler = std::function<void(uint64_t ctx, uint64_t ctx2,
		const std::wstring& hookCode, const std::wstring& hookName)>;
	// Pesan info/host (konsol, peringatan) -> bisa ditampilkan di status/log.
	using InfoHandler = std::function<void(int type, const std::wstring& message)>;

	static LunaBridge& Instance();

	// Muat LunaHost64.dll + panggil Luna_Start. Aman dipanggil berkali-kali (idempoten).
	// Mengembalikan false bila DLL/komponen tidak ada atau gagal dimuat.
	bool EnsureStarted();
	bool IsAvailable() const { return dll != nullptr && started; }

	// Path folder LunaHook (default: <folder exe>\LunaHook). Set sebelum EnsureStarted bila perlu.
	void SetComponentDir(std::wstring dir) { componentDir = std::move(dir); }
	const std::wstring& ComponentDir() const { return componentDir; }

	void SetOutputHandler(OutputHandler h) { outputHandler = std::move(h); }
	void SetHookHandler(HookHandler h) { hookHandler = std::move(h); }
	void SetInfoHandler(InfoHandler h) { infoHandler = std::move(h); }

	// Sambungkan ke game & inject bila perlu (via LunaSubProcess). Non-blocking.
	// Mengembalikan false bila gagal memulai injeksi.
	bool ConnectAndInject(DWORD processId);

	// Hook manual: masukkan hook code (mis. "/HB8@4E7B0" atau "HQ..."). Butuh sudah ter-inject.
	bool InsertHookCode(DWORD processId, const std::wstring& hookCode);

	// Lepas proses.
	void Detach(DWORD processId);

	// Terakhir kali ada error yang bisa ditampilkan ke user.
	const std::wstring& LastError() const { return lastError; }

	// Apakah proses 64-bit? (untuk pilih LunaHook64/32 & subprocess). true=64bit.
	static bool ProcessIs64Bit(DWORD processId);

private:
	LunaBridge() = default;
	LunaBridge(const LunaBridge&) = delete;
	LunaBridge& operator=(const LunaBridge&) = delete;

	std::wstring ResolveComponentDir();
	bool RunInject(DWORD processId, bool is64);

	// ---- pointer fungsi LunaHost ----
	using PROC_Start = void(*)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
	using PROC_Connect = void(*)(DWORD);
	using PROC_Detach = void(*)(DWORD);
	using PROC_CheckInject = bool(*)(DWORD);
	using PROC_InsertHook = bool(*)(DWORD, const wchar_t*);
	using PROC_Settings = void(*)(int, int, int, int, bool);
	using PROC_AllocString = void*(*)(const wchar_t*);

	HMODULE dll = nullptr;
	bool started = false;
	std::wstring componentDir;
	std::wstring lastError;

	PROC_Start p_Start = nullptr;
	PROC_Connect p_Connect = nullptr;
	PROC_Detach p_Detach = nullptr;
	PROC_CheckInject p_CheckInject = nullptr;
	PROC_InsertHook p_InsertHook = nullptr;
	PROC_Settings p_Settings = nullptr;
	PROC_AllocString p_AllocString = nullptr;

	OutputHandler outputHandler;
	HookHandler hookHandler;
	InfoHandler infoHandler;

	// callback statis C -> teruskan ke Instance()
	static void __cdecl OnConnect(DWORD pid);
	static void __cdecl OnDisconnect(DWORD pid);
	static void __cdecl OnNewHook(const wchar_t* hookCode, const char* hookName, ThreadParam tp, bool embeddable);
	static void __cdecl OnRemoveHook(const wchar_t* hookCode, const char* hookName, ThreadParam tp);
	static void __cdecl OnOutput(const wchar_t* hookCode, const char* hookName, ThreadParam tp, const wchar_t* output);
	static void __cdecl OnHostInfo(int type, const wchar_t* message);
	static void __cdecl OnHookInsert(DWORD pid, uint64_t addr, const wchar_t* hookCode);
	static void __cdecl OnEmbed(const wchar_t* text, ThreadParam tp);
	static void* __cdecl OnI18N(const wchar_t* text);
};
