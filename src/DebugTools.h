#pragma once

#ifdef DEBUG

#include <Dbghelp.h>
#include <tlhelp32.h>

class DebugTools {
public:

	static bool WaitDebuggerAttach(DWORD timeoutMs = INFINITE, DWORD checkIntervalMs = 100)
	{
		const DWORD startTick = GetTickCount();

		while (true)
		{
			if (IsDebuggerPresent())
			{
				return true;
			}

			BOOL remoteDebugger = FALSE;
			CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);

			if (remoteDebugger)
			{
				return true;
			}

			if (timeoutMs != INFINITE)
			{
				const DWORD elapsed = GetTickCount() - startTick;

				if (elapsed >= timeoutMs)
				{
					return false;
				}
			}

			Sleep(checkIntervalMs);
		}
	}

	// 等待调试器附加后自动触发断点
	static bool WaitDebuggerAttachAndBreak(
		DWORD timeoutMs = INFINITE,
		DWORD checkIntervalMs = 100)
	{
		if (WaitDebuggerAttach(timeoutMs, checkIntervalMs))
		{
			__debugbreak();
			return true;
		}

		return false;
	}


	static bool DetachFromDebugger()
	{
		auto GetDebuggerProcessId = [](DWORD dwSelfProcessId) -> DWORD
			{
				DWORD dwParentProcessId = -1;
				HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
				PROCESSENTRY32 pe32;
				pe32.dwSize = sizeof(PROCESSENTRY32);
				Process32First(hSnapshot, &pe32);
				do
				{
					if (pe32.th32ProcessID == dwSelfProcessId)
					{
						dwParentProcessId = pe32.th32ParentProcessID;
						break;
					}
				} while (Process32Next(hSnapshot, &pe32));
				CloseHandle(hSnapshot);
				return dwParentProcessId;
			};

		HMODULE hModule = LoadLibrary("ntdll.dll");
		if (hModule != NULL)
		{
			auto const NtRemoveProcessDebug =
				(NTSTATUS(__stdcall*)(HANDLE, HANDLE))GetProcAddress(hModule, "NtRemoveProcessDebug");
			auto const NtSetInformationDebugObject =
				(NTSTATUS(__stdcall*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(hModule, "NtSetInformationDebugObject");
			auto const NtQueryInformationProcess =
				(NTSTATUS(__stdcall*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(hModule, "NtQueryInformationProcess");
			auto const NtClose =
				(NTSTATUS(__stdcall*)(HANDLE))GetProcAddress(hModule, "NtClose");

			HANDLE hDebug;
			HANDLE hCurrentProcess = GetCurrentProcess();
			NTSTATUS status = NtQueryInformationProcess(hCurrentProcess, 30, &hDebug, sizeof(HANDLE), 0);
			if (0 <= status)
			{
				ULONG killProcessOnExit = FALSE;
				status = NtSetInformationDebugObject(
					hDebug,
					1,
					&killProcessOnExit,
					sizeof(ULONG),
					NULL
				);
				if (0 <= status)
				{
					const auto pid = GetDebuggerProcessId(GetProcessId(hCurrentProcess));
					status = NtRemoveProcessDebug(hCurrentProcess, hDebug);
					if (0 <= status)
					{
						HANDLE hDbgProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
						if (INVALID_HANDLE_VALUE != hDbgProcess)
						{
							BOOL ret = TerminateProcess(hDbgProcess, EXIT_SUCCESS);
							CloseHandle(hDbgProcess);
							return ret;
						}
					}
				}
				NtClose(hDebug);
			}
			FreeLibrary(hModule);
		}

		return false;
	}
};

#endif