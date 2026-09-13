#pragma once

#include "textthread.h"

namespace Host
{
	using ProcessEventHandler = void(*)(DWORD);
	using ThreadEventHandler = std::function<void(TextThread&)>;
	using HookEventHandler = std::function<void(HookParam, std::wstring text)>;
	void Start(ProcessEventHandler Connect, ProcessEventHandler Disconnect, ThreadEventHandler Create, ThreadEventHandler Destroy, TextThread::OutputCallback Output);

	void InjectProcess(DWORD processId);
	void DetachProcess(DWORD processId);

	void InsertHook(DWORD processId, HookParam hp);
	void RemoveHook(DWORD processId, uint64_t address);
	void FindHooks(DWORD processId, SearchParam sp, HookEventHandler HookFound = {});

	TextThread* GetThread(int64_t handle);
	TextThread& GetThread(ThreadParam tp);

	void AddConsoleOutput(std::wstring text);

	// LunaHook integration (Opsi B): external text source. Creates/uses a synthetic
	// TextThread so LunaHook text flows through the SAME pipeline as normal hooks
	// (scoring -> translation extensions -> cards/overlay/history).
	// ctx/ctx2 are used to distinguish different LunaHook hook contexts.
	TextThread& GetLunaThread(uint64_t ctx, uint64_t ctx2, std::wstring name);
	void AddLunaSentence(uint64_t ctx, uint64_t ctx2, std::wstring name, std::wstring sentence);
	void RemoveLunaThreads();

	inline int defaultCodepage = SHIFT_JIS;

	// processId 0xF10A ("FLOA"/Luna sentinel) marks synthetic LunaHook threads.
	constexpr DWORD LUNA_PROCESS = 0xF10A;
	constexpr ThreadParam console{ 0, -1LL, -1LL, -1LL }, clipboard{ 0, 0, -1LL, -1LL };
}
