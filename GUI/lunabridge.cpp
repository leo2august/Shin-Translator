#include "lunabridge.h"
#include <filesystem>
#include <string>
#include <shellapi.h>
#include <QSettings>

// Catatan: LunaTranslator (texthook.py) mendaftarkan callback dengan CFUNCTYPE
// (konvensi cdecl), dan fungsi export dipanggil dengan cdecl juga. Kita cocokkan
// dengan __cdecl agar ABI sesuai pada x86; pada x64 hanya ada satu konvensi.

LunaBridge& LunaBridge::Instance()
{
	static LunaBridge instance;
	return instance;
}

std::wstring LunaBridge::ResolveComponentDir()
{
	if (!componentDir.empty()) return componentDir;
	wchar_t path[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(path).parent_path();
	// Prioritas: <exe>\LunaHook, lalu <exe> (kalau file ditaruh berdampingan).
	std::filesystem::path lunaDir = exeDir / L"LunaHook";
	if (std::filesystem::exists(lunaDir / L"LunaHost64.dll")) componentDir = lunaDir.wstring();
	else componentDir = exeDir.wstring();
	return componentDir;
}

bool LunaBridge::EnsureStarted()
{
	if (started && dll) return true;

	std::wstring dir = ResolveComponentDir();
	std::filesystem::path hostPath = std::filesystem::path(dir) / L"LunaHost64.dll";
	if (!std::filesystem::exists(hostPath))
	{
		lastError = L"LunaHost64.dll tidak ditemukan di: " + dir;
		return false;
	}

	// Tambahkan folder komponen ke DLL search path agar dependensi (LunaHook DLL,
	// runtime) ketemu, lalu muat via full path.
	SetDllDirectoryW(dir.c_str());
	dll = LoadLibraryExW(hostPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!dll)
	{
		lastError = L"Gagal memuat LunaHost64.dll (err=" + std::to_wstring(GetLastError()) + L")";
		return false;
	}

	p_Start = (PROC_Start)GetProcAddress(dll, "Luna_Start");
	p_Connect = (PROC_Connect)GetProcAddress(dll, "Luna_ConnectProcess");
	p_Detach = (PROC_Detach)GetProcAddress(dll, "Luna_DetachProcess");
	p_CheckInject = (PROC_CheckInject)GetProcAddress(dll, "Luna_CheckIfNeedInject");
	p_InsertHook = (PROC_InsertHook)GetProcAddress(dll, "Luna_InsertHookCode");
	p_Settings = (PROC_Settings)GetProcAddress(dll, "Luna_Settings");
	p_AllocString = (PROC_AllocString)GetProcAddress(dll, "Luna_AllocString");

	if (!p_Start || !p_Connect || !p_Settings)
	{
		lastError = L"Export LunaHost tidak lengkap (Luna_Start/ConnectProcess/Settings).";
		FreeLibrary(dll);
		dll = nullptr;
		return false;
	}

	// Urutan 9 callback = persis LunaTranslator texthook.py.
	p_Start(
		(void*)&LunaBridge::OnConnect,
		(void*)&LunaBridge::OnDisconnect,
		(void*)&LunaBridge::OnNewHook,
		(void*)&LunaBridge::OnRemoveHook,
		(void*)&LunaBridge::OnOutput,
		(void*)&LunaBridge::OnHostInfo,
		(void*)&LunaBridge::OnHookInsert,
		(void*)&LunaBridge::OnEmbed,
		(void*)&LunaBridge::OnI18N);

	// flushDelay lebih responsif (150ms) - LunaHook merakit kalimat secara internal
	// jadi delay kecil aman & teks muncul jauh lebih cepat. codepage=932 (SHIFT-JIS).
	int flushMs = 150;
	{
		// Hormati setelan pengguna bila ada (Settings > Advanced/Performance).
		QSettings s("Textractor.ini", QSettings::IniFormat);
		flushMs = s.value("Luna flush delay", 150).toInt();
		if (flushMs < 30) flushMs = 30;
		if (flushMs > 2000) flushMs = 2000;
	}
	p_Settings(flushMs, 932, 3000, 1000000, false);

	started = true;
	lastError.clear();
	return true;
}

bool LunaBridge::ProcessIs64Bit(DWORD processId)
{
#ifdef _WIN64
	// OS 64-bit. Proses 64-bit kecuali WOW64 (32-bit di bawah WOW64).
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!h) h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
	if (!h) return true; // asumsi 64-bit bila tak bisa cek
	BOOL isWow64 = FALSE;
	IsWow64Process(h, &isWow64);
	CloseHandle(h);
	return !isWow64;
#else
	(void)processId;
	return false; // aplikasi 32-bit -> perlakukan target sebagai 32-bit
#endif
}

bool LunaBridge::RunInject(DWORD processId, bool is64)
{
	std::wstring dir = ResolveComponentDir();
	// LunaSubProcess & LunaHook DLL bisa di folder komponen atau folder induknya.
	auto findFile = [&](const std::wstring& name) -> std::wstring
	{
		std::filesystem::path a = std::filesystem::path(dir) / name;
		if (std::filesystem::exists(a)) return a.wstring();
		std::filesystem::path b = std::filesystem::path(dir).parent_path() / name;
		if (std::filesystem::exists(b)) return b.wstring();
		return {};
	};

	std::wstring subName = is64 ? L"LunaSubProcess64.exe" : L"LunaSubProcess32.exe";
	std::wstring dllName = is64 ? L"LunaHook64.dll" : L"LunaHook32.dll";
	std::wstring subPath = findFile(subName);
	std::wstring dllPath = findFile(dllName);
	if (subPath.empty()) { lastError = subName + L" tidak ditemukan."; return false; }
	if (dllPath.empty()) { lastError = dllName + L" tidak ditemukan."; return false; }

	// Perintah:  LunaSubProcess<bit>.exe dllinject <pid> <path\LunaHook<bit>.dll>
	std::wstring cmd = L"\"" + subPath + L"\" dllinject " + std::to_wstring(processId)
		+ L" \"" + dllPath + L"\"";

	STARTUPINFOW si{}; si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi{};
	std::wstring mutableCmd = cmd; // CreateProcessW butuh buffer non-const
	BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	if (!ok)
	{
		DWORD e = GetLastError();
		if (e == ERROR_ELEVATION_REQUIRED || e == ERROR_ACCESS_DENIED)
		{
			// Coba elevated (runas) via ShellExecute.
			std::wstring params = L"dllinject " + std::to_wstring(processId) + L" \"" + dllPath + L"\"";
			HINSTANCE r = ShellExecuteW(nullptr, L"runas", subPath.c_str(), params.c_str(), nullptr, SW_HIDE);
			if ((INT_PTR)r <= 32) { lastError = L"Injeksi elevated gagal (kode " + std::to_wstring((INT_PTR)r) + L")."; return false; }
			return true;
		}
		lastError = L"Gagal menjalankan LunaSubProcess (err=" + std::to_wstring(e) + L").";
		return false;
	}
	WaitForSingleObject(pi.hProcess, 15000); // injeksi biasanya cepat
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

bool LunaBridge::ConnectAndInject(DWORD processId)
{
	if (!EnsureStarted()) return false;

	// Daftarkan proses ke host lebih dulu.
	p_Connect(processId);

	bool need = true;
	if (p_CheckInject) need = p_CheckInject(processId);
	if (need)
	{
		bool is64 = ProcessIs64Bit(processId);
		if (!RunInject(processId, is64)) return false;
		// Sambungkan ulang setelah injeksi agar host mengaitkan pipa game.
		p_Connect(processId);
	}
	return true;
}

bool LunaBridge::InsertHookCode(DWORD processId, const std::wstring& hookCode)
{
	if (!EnsureStarted() || !p_InsertHook) { lastError = L"LunaHook belum siap."; return false; }
	return p_InsertHook(processId, hookCode.c_str());
}

void LunaBridge::Detach(DWORD processId)
{
	if (started && p_Detach) p_Detach(processId);
}

// ---------------- callback statis ----------------

void __cdecl LunaBridge::OnConnect(DWORD) {}
void __cdecl LunaBridge::OnDisconnect(DWORD) {}

void __cdecl LunaBridge::OnNewHook(const wchar_t* hookCode, const char* hookName, ThreadParam tp, bool)
{
	auto& self = Instance();
	if (self.hookHandler)
	{
		std::wstring name;
		if (hookName) { std::string s(hookName); name.assign(s.begin(), s.end()); }
		self.hookHandler(tp.ctx, tp.ctx2, hookCode ? hookCode : L"", name);
	}
}

void __cdecl LunaBridge::OnRemoveHook(const wchar_t*, const char*, ThreadParam) {}

void __cdecl LunaBridge::OnOutput(const wchar_t*, const char* hookName, ThreadParam tp, const wchar_t* output)
{
	auto& self = Instance();
	if (self.outputHandler && output && *output)
	{
		std::wstring name;
		if (hookName) { std::string s(hookName); name.assign(s.begin(), s.end()); }
		self.outputHandler(tp.ctx, tp.ctx2, name, output);
	}
}

void __cdecl LunaBridge::OnHostInfo(int type, const wchar_t* message)
{
	auto& self = Instance();
	if (self.infoHandler) self.infoHandler(type, message ? message : L"");
}

void __cdecl LunaBridge::OnHookInsert(DWORD, uint64_t, const wchar_t*) {}
void __cdecl LunaBridge::OnEmbed(const wchar_t*, ThreadParam) {}

void* __cdecl LunaBridge::OnI18N(const wchar_t* text)
{
	// Kembalikan salinan string alokasi host (identitas: tak menerjemahkan i18n).
	auto& self = Instance();
	if (self.p_AllocString && text) return self.p_AllocString(text);
	return nullptr;
}
