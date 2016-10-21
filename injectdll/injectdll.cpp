#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <tchar.h>
#include "injectlib.h"

typedef HMODULE(WINAPI *typeLoadLibrary)(LPCSTR);
typedef BOOL(WINAPI *typeFreeLibrary)(HMODULE hLibModule);
typedef FARPROC(WINAPI *typeGetProcAddress)(HMODULE, LPCSTR);
typedef void (WINAPI *typeFunc)(void);

struct FuncInfo
{
	typeLoadLibrary LoadLibrary;
	typeFreeLibrary FreeLibrary;
	typeGetProcAddress GetProcAddress;
	char dll[MAX_PATH];
	char func[16];
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
	HMODULE dll = info->LoadLibrary(info->dll);
	if (dll)
	{
		typeFunc func = (typeFunc)info->GetProcAddress(dll, info->func);
		if (func)
			func();
	}
	info->FreeLibrary(dll);
	return 0;
}

int WINAPI EndFunc()
{
	return 0;
}

void InjectDLL(const char* pid, const char* pathDll, const char* func)
{
	bool res = false;
	FuncInfo info;
	strcpy(info.dll, pathDll);
	if(func)
		strcpy(info.func, func);

	HMODULE kernel = GetModuleHandleA("kernel32.dll");
	info.LoadLibrary = (typeLoadLibrary)GetProcAddress(kernel, "LoadLibraryA");
	info.GetProcAddress = (typeGetProcAddress)GetProcAddress(kernel, "GetProcAddress");
	info.FreeLibrary = (typeFreeLibrary)GetProcAddress(kernel, "FreeLibrary");

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
			printf("dll injected in process[%s]\n", pid);
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
	if (argc !=3 && argc != 4)
	{
		printf("Usage:\n\tinjectdll.exe pid dllpath [func]");
		return 0;
	}

	EnableProcessPrivilege(GetCurrentProcess(), SE_DEBUG_NAME, TRUE);
	if (argc == 3)
		InjectDLL(argv[1], argv[2], NULL);
	if (argc == 4)
		InjectDLL(argv[1], argv[2], argv[3]);

	return 1;
}
