#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <tchar.h>
#include "injectlib.h"

typedef HMODULE(WINAPI *typeLoadLibrary)(LPCSTR);
typedef BOOL(WINAPI *typeFreeLibrary)(HMODULE hLibModule);
typedef FARPROC(WINAPI *typeGetProcAddress)(HMODULE, LPCSTR);
typedef void (WINAPI *typeExitThread)(DWORD);
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

int main(int argc, char* argv[])
{
	if (argc !=3 && argc != 4)
	{
		printf("Usage:\n\tinjectdll.exe pid dllpath [func]");
		return 0;
	}

	if (argc == 3)
		InjectDLL(argv[1], argv[2], NULL);
	if (argc == 4)
		InjectDLL(argv[1], argv[2], argv[3]);

	return 1;
}
