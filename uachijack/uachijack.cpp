#include <Windows.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#pragma comment(lib, "Shlwapi.lib")


#define NT_SUCCESS(Status)((NTSTATUS)(Status) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)

#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_KERNEL_HANDLE 0x00000200L

typedef LONG NTSTATUS;

typedef struct _IO_STATUS_BLOCK
{
	NTSTATUS Status;
	ULONG Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
}UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES
{
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef NTSTATUS(CALLBACK* ZWOPENSECTION)(
	OUT PHANDLE SectionHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes
	);

typedef VOID(CALLBACK* RTLINITUNICODESTRING)(
	IN OUT PUNICODE_STRING DestinationString,
	IN PCWSTR SourceString
	);

RTLINITUNICODESTRING RtlInitUnicodeString;
ZWOPENSECTION ZwOpenSection;


PWCHAR Ansi2Uni(LPCSTR lpMultiByteStr)
{
	int		nUniLen = 0;
	PWCHAR  pUnicode = NULL;
	nUniLen = MultiByteToWideChar(CP_ACP, 0, lpMultiByteStr, -1, NULL, 0);
	pUnicode = (PWCHAR)malloc((nUniLen + 1) * sizeof(WCHAR));
	memset(pUnicode, 0, (nUniLen + 1) * sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, lpMultiByteStr, -1, (LPWSTR)pUnicode, nUniLen);
	return  pUnicode;
}

void InitNtFunc()
{
	HMODULE hModule = GetModuleHandle(_T("ntdll.dll"));
	RtlInitUnicodeString = (RTLINITUNICODESTRING)GetProcAddress(hModule, "RtlInitUnicodeString");
	ZwOpenSection = (ZWOPENSECTION)GetProcAddress(hModule, "ZwOpenSection");
}

BOOL CheckKnownDllsExists(LPCWSTR dllName)
{
	HANDLE hSection = NULL;
	NTSTATUS status;
	OBJECT_ATTRIBUTES attributes;
	UNICODE_STRING us;
	WCHAR known_path[MAX_PATH] = { 0 };

	wsprintfW(known_path, L"\\KnownDlls\\%s", dllName);

	RtlInitUnicodeString(&us, known_path);

	attributes.Length = sizeof(OBJECT_ATTRIBUTES);
	attributes.RootDirectory = NULL;
	attributes.ObjectName = &us;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	attributes.SecurityDescriptor = NULL;
	attributes.SecurityQualityOfService = NULL;

	status = ZwOpenSection(&hSection, SECTION_QUERY, &attributes);

	if (hSection)
		CloseHandle(hSection);

	return NT_SUCCESS(status);
}

DWORD Rva2Offset(DWORD rva, PIMAGE_SECTION_HEADER psh, PIMAGE_NT_HEADERS pnt)
{
	size_t i = 0;
	PIMAGE_SECTION_HEADER pSeh;
	if (rva == 0)
		return (rva);
	pSeh = psh;
	for (i = 0; i < pnt->FileHeader.NumberOfSections; i++)
	{
		if (rva >= pSeh->VirtualAddress && rva < pSeh->VirtualAddress + pSeh->Misc.VirtualSize)
			break;
		pSeh++;
	}
	return (rva - pSeh->VirtualAddress + pSeh->PointerToRawData);
}

void ParseImportTable(LPCTSTR file)
{
	HANDLE handle = CreateFile(file, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	DWORD byteread, size = GetFileSize(handle, NULL);
	PVOID virtualpointer = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	ReadFile(handle, virtualpointer, size, &byteread, NULL);
	CloseHandle(handle);

	PIMAGE_NT_HEADERS           ntheaders = (PIMAGE_NT_HEADERS)(PCHAR(virtualpointer) + PIMAGE_DOS_HEADER(virtualpointer)->e_lfanew);
	PIMAGE_SECTION_HEADER       pSech = IMAGE_FIRST_SECTION(ntheaders);
	PIMAGE_IMPORT_DESCRIPTOR    pImportDescriptor;

	if (ntheaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size != 0)
	{
		pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)virtualpointer + Rva2Offset(ntheaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, pSech, ntheaders));

		while (pImportDescriptor->Name != NULL)
		{
			char* name = (PCHAR)((DWORD_PTR)virtualpointer + Rva2Offset(pImportDescriptor->Name, pSech, ntheaders));

			if (!CheckKnownDllsExists(Ansi2Uni(name)))
				printf("%s\n", name);
	
			pImportDescriptor++;
		}
	}

	if (virtualpointer)
		VirtualFree(virtualpointer, size, MEM_DECOMMIT);
}

BOOL CALLBACK EnumResourceNameCallback_Dir(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)
{
	HRSRC hResInfo = FindResource(hModule, lpName, lpType);
	DWORD cbResource = SizeofResource(hModule, hResInfo);

	HGLOBAL hResData = LoadResource(hModule, hResInfo);
	const BYTE *pResource = (const BYTE *)LockResource(hResData);

	char* manifest = new char[cbResource + 1];
	memcpy(manifest, pResource, cbResource);
	manifest[cbResource] = '\0';

	if (strstr(manifest, "requireAdministrator"))
	{
		if (strstr(manifest, "true</autoElevate>"))
		{
			_tprintf(_T("%s\n"), (TCHAR*)lParam);
			ParseImportTable((TCHAR*)lParam);
		}
	}

	UnlockResource(hResData);
	FreeResource(hResData);

	return TRUE;
}

void EnumDirectory(LPCTSTR lpszDir)
{
	HANDLE hFind = NULL;
	WIN32_FIND_DATA wfd = { 0 };
	TCHAR szTemp[MAX_PATH] = { 0 };

	_stprintf(szTemp, _T("%s\\*"), lpszDir);

	hFind = FindFirstFile(szTemp, &wfd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (_tcsicmp(wfd.cFileName, _T(".")) == 0 || _tcsicmp(wfd.cFileName, _T("..")) == 0)
			continue;

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			TCHAR szSubDir[MAX_PATH] = { 0 };
			_stprintf(szSubDir, _T("%s\\%s"), lpszDir, wfd.cFileName);
			EnumDirectory(szSubDir);
		}
		else
		{
			TCHAR szFile[MAX_PATH] = { 0 };
			_stprintf(szFile, _T("%s\\%s"), lpszDir, wfd.cFileName);
			TCHAR* ext = PathFindExtension(szFile);
			if (_tcsicmp(ext, _T(".exe")) == 0)
			{
				HMODULE hModule = LoadLibraryEx(szFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
				EnumResourceNames(hModule, RT_MANIFEST, EnumResourceNameCallback_Dir, (LONG_PTR)szFile);
				FreeLibrary(hModule);
			}
		}
	} while (FindNextFile(hFind, &wfd));

	FindClose(hFind);
}

BOOL CALLBACK EnumResourceNameCallback_file(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)
{
	HRSRC hResInfo = FindResource(hModule, lpName, lpType);
	DWORD cbResource = SizeofResource(hModule, hResInfo);

	HGLOBAL hResData = LoadResource(hModule, hResInfo);
	const BYTE *pResource = (const BYTE *)LockResource(hResData);

	TCHAR filename[MAX_PATH];
	if (IS_INTRESOURCE(lpName))
		_stprintf_s(filename, _T("%s_#%d.manifest"), (char*)lParam, lpName);
	else
		_stprintf_s(filename, _T("%s_%s.manifest"), (char*)lParam, lpName);

	FILE *f = _tfopen(filename, _T("wb"));
	fwrite(pResource, cbResource, 1, f);
	fclose(f);

	UnlockResource(hResData);
	FreeResource(hResData);

	return TRUE;
}

void EnumFile(LPCTSTR file)
{
	TCHAR* fileName = PathFindFileName(file);
	HMODULE hModule = LoadLibraryEx(file, NULL, LOAD_LIBRARY_AS_DATAFILE);
	EnumResourceNames(hModule, RT_MANIFEST, EnumResourceNameCallback_file, (LONG_PTR)fileName);
	FreeLibrary(hModule);
}

void _tmain(int argc, TCHAR* argv[])
{
	InitNtFunc();
	if (_tcsicmp(argv[1], _T("-f")) == 0)
		EnumFile(argv[2]);
	else if (_tcsicmp(argv[1], _T("-d")) == 0)
		EnumDirectory(_T("C:\\Windows\\System32"));
}