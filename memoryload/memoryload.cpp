#include <Windows.h>
#include <stdio.h>

//bin2hex.exe testexe.exe testexe.h hexcode
#ifdef _M_X64
#include "testexe64.h"
#else
#include "testexe.h"
#endif

typedef int(__stdcall * EntryPoint)(int argc, char* argv[]);


BOOL WINAPI MemoryLoad(void* buf, int argc, char* argv[])
{
	PCHAR pBuf = (PCHAR)buf;
	PIMAGE_DOS_HEADER      pDosHeader = (PIMAGE_DOS_HEADER)pBuf;
	PIMAGE_FILE_HEADER     pFileHeader = (PIMAGE_FILE_HEADER)(pBuf + pDosHeader->e_lfanew + 4);
	PIMAGE_OPTIONAL_HEADER pOptionalHeader = (PIMAGE_OPTIONAL_HEADER)(pFileHeader + 1);
	PIMAGE_SECTION_HEADER  pSectionHeader = (PIMAGE_SECTION_HEADER)((PCHAR)pOptionalHeader + sizeof(IMAGE_OPTIONAL_HEADER));

	LPBYTE Mapping = (LPBYTE)VirtualAlloc(NULL, pOptionalHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (Mapping == NULL)
		return FALSE;

	for (int i = 0; i < pFileHeader->NumberOfSections; i++, pSectionHeader++)
	{
		LPVOID VirtualAddress = (LPVOID)((LPBYTE)Mapping + pSectionHeader->VirtualAddress);

		int count = pSectionHeader->SizeOfRawData;
		void * dst = VirtualAddress;
		const void * src = pBuf + pSectionHeader->PointerToRawData;

		while (count--)
		{
			*(char *)dst = *(char *)src;
			dst = (char *)dst + 1;
			src = (char *)src + 1;
		}
	}

	if (pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size)
	{
		PIMAGE_BASE_RELOCATION Reloc = (PIMAGE_BASE_RELOCATION)((ULONG_PTR)(pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress) + (LPBYTE)(Mapping));
		LPBYTE dwImageBase = (LPBYTE)Mapping;
		ULONG_PTR iOffsetRlc = (LPBYTE)Mapping - (LPBYTE)pOptionalHeader->ImageBase;
		ULONG Size = pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

		LPBYTE vaddr;
		DWORD  dwCount, dwMiniOffset, dwRelocateType;
		WORD *items = NULL;

		while (Reloc->VirtualAddress != NULL)
		{
			vaddr = dwImageBase + Reloc->VirtualAddress;
			dwCount = (Reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) >> 1;
			items = (WORD *)((char *)Reloc + sizeof(IMAGE_BASE_RELOCATION));

			for (DWORD i = 0; i < dwCount; ++i)
			{
				dwMiniOffset = items[i] & 0x0fff;
				dwRelocateType = items[i] >> 12;
				if (dwRelocateType == IMAGE_REL_BASED_HIGHLOW || IMAGE_REL_BASED_DIR64 == dwRelocateType)
				{
					(*(ULONG_PTR *)(vaddr + dwMiniOffset)) += iOffsetRlc;
				}
			}

			Reloc = (PIMAGE_BASE_RELOCATION)(items + dwCount);
		}
	}

	if (pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)
	{
		PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)((ULONG_PTR)(pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) + (LPBYTE)(Mapping));
		PIMAGE_THUNK_DATA pOrgThunk, pFirstThunk;
		PIMAGE_IMPORT_BY_NAME pImportName;

		while (pImport->OriginalFirstThunk != NULL)
		{
			char *name = (char*)Mapping + pImport->Name;

			FARPROC fpFun;
			HINSTANCE hInstance = LoadLibraryA(name);
			if (hInstance == NULL)
				return FALSE;

			pOrgThunk = (PIMAGE_THUNK_DATA)((LPBYTE)Mapping + pImport->OriginalFirstThunk);
			pFirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)Mapping + pImport->FirstThunk);

			while (*(DWORD*)pOrgThunk != NULL)
			{
				if ((pOrgThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) || (pOrgThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64))
					fpFun = GetProcAddress(hInstance, (LPCSTR)(pOrgThunk->u1.Ordinal & 0x0000ffff));
				else
				{
					pImportName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)Mapping + pOrgThunk->u1.AddressOfData);
					fpFun = GetProcAddress(hInstance, (LPCSTR)pImportName->Name);
				}

				pFirstThunk->u1.Ordinal = (ULONG_PTR)fpFun;
				pFirstThunk++;
				pOrgThunk++;
			}
			pImport++;
		}
	}

	DWORD lpflOldProtect = 0;
	VirtualProtect((void*)Mapping, pOptionalHeader->SizeOfImage, PAGE_EXECUTE_READWRITE, &lpflOldProtect);
	LPVOID entry = (LPVOID)((LPBYTE)Mapping + pOptionalHeader->AddressOfEntryPoint);

	EntryPoint pEntryFunc = (EntryPoint)entry;
	pEntryFunc(argc, argv);

	return TRUE;
}

int main(int argc, char* argv[])
{
	MemoryLoad(hexcode, argc, argv);
	return 0;
}
