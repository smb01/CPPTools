#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <Commctrl.h>
#include <tchar.h>
#include "appinfo.h"
#pragma comment(lib, "Shlwapi.lib")

#define MAX_BUF 260

typedef struct
{
	BOOL Vulnerable;
	WCHAR DisplayName[MAX_BUF];
	WCHAR UninstallString[MAX_PATH];
}UninstallItem, *pUninstallItem;

LPSTR Unicode2Ansi(LPCWSTR lpWideCharStr)
{
	int		nAnsiLen = 0;
	LPSTR	pAnsi = NULL;
	nAnsiLen = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, -1, NULL, 0, NULL, NULL);
	pAnsi = (LPSTR)malloc(nAnsiLen + 1);
	memset(pAnsi, 0, nAnsiLen + 1);
	WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, -1, pAnsi, nAnsiLen, NULL, NULL);
	return pAnsi;
}

HWND FindNestedWindowFromClassName(HWND OutmostWindow, PWCHAR *ClassNames, DWORD ClassCount)
{
	HWND ParentHwnd = OutmostWindow;
	HWND ChildHwnd = NULL;
	for (DWORD i = 0; i < ClassCount; i++)
	{
		do
		{
			ChildHwnd = FindWindowEx(ParentHwnd, NULL, ClassNames[i], NULL);
		} while (!ChildHwnd);
		ParentHwnd = ChildHwnd;
	}
	return ChildHwnd;
}

VOID GetListedNames(HANDLE hProcess, HWND SysListView32Hwnd, pUninstallItem pItems, DWORD ItemCount)
{
	WCHAR TextBuf[MAX_BUF] = { 0 };
	PWCHAR pText = (PWCHAR)VirtualAllocEx(hProcess, NULL, MAX_BUF, MEM_COMMIT, PAGE_READWRITE);
	LPLVITEMW plvitem = (LPLVITEMW)VirtualAllocEx(hProcess, NULL, sizeof(LVITEM), MEM_COMMIT, PAGE_READWRITE);
	LVITEMW lvitem;
	lvitem.cchTextMax = MAX_BUF;
	lvitem.iSubItem = 0;
	lvitem.pszText = pText;
	WriteProcessMemory(hProcess, plvitem, &lvitem, sizeof(LVITEMW), NULL);
	for (DWORD i = 0; i < ItemCount; i++)
	{
		SendMessage(SysListView32Hwnd, LVM_GETITEMTEXT, i, (LPARAM)plvitem);
		ReadProcessMemory(hProcess, pText, TextBuf, MAX_BUF, NULL);
		pItems[i].Vulnerable = FALSE;
		lstrcpyn(pItems[i].DisplayName, TextBuf, MAX_BUF);
	}
}

BOOL DBClickItem(HANDLE hProcess, HWND SysListView32Hwnd, DWORD ItemIndex)
{
	if (!SysListView32Hwnd || !hProcess)
		return FALSE;
	PRECT pRect = (PRECT)VirtualAllocEx(hProcess, NULL, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE);
	if (!pRect)
		return FALSE;
	RECT Rect;
	Rect.left = LVIR_BOUNDS;
	if (!WriteProcessMemory(hProcess, pRect, &Rect, sizeof(RECT), NULL))
		return FALSE;
	SendMessage(SysListView32Hwnd, LVM_GETITEMRECT, ItemIndex, (LPARAM)pRect);
	if (!ReadProcessMemory(hProcess, pRect, &Rect, sizeof(RECT), NULL))
		return FALSE;
	DWORD Pos = ((Rect.top + Rect.bottom) / 2 << 16) + (Rect.left + 30);
	PostMessage(SysListView32Hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (LPARAM)Pos);
	PostMessage(SysListView32Hwnd, WM_LBUTTONUP, NULL, (LPARAM)Pos);
	PostMessage(SysListView32Hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, (LPARAM)Pos);
	PostMessage(SysListView32Hwnd, WM_LBUTTONUP, NULL, (LPARAM)Pos);
	return TRUE;
}

HANDLE WindowToProcess(HWND hWnd)
{
	DWORD dwPID = 0;
	DWORD dwThreadID = GetWindowThreadProcessId(hWnd, &dwPID);
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID);
}

void wmain(int argc, wchar_t* argv[])
{
	bool bList = false;
	int nIndex = -1;
	vector<ApplicationInfoA> vAppInfo;
	vector<ApplicationInfoA> vAppInfo_wow64;
	WCHAR WinClassName[MAX_BUF] = { 0 };
	WCHAR WinText[MAX_BUF] = { 0 };
	HWND Hwnd = NULL;
	PWCHAR WindowClasses[] = { L"ShellTabWindowClass", L"DUIViewWndClassName", L"DirectUIHWND", L"CtrlNotifySink", L"SHELLDLL_DefView", L"SysListView32" };

	if (wcsicmp(argv[1], L"-list") == 0)
		bList = true;
	else if (wcsicmp(argv[1], L"-exp") == 0)
		nIndex = _wtoi(argv[2]);
	
	if (!bList && nIndex == 0)
		return;

	GetAllInstalledAppInfoA("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", vAppInfo);
	GetAllInstalledAppInfoA("SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", vAppInfo_wow64);

	WinExec("rundll32.exe shell32.dll Control_RunDLL appwiz.cpl 2", SW_HIDE);

	for (int i = 0; i < 5; i++)
	{
		Hwnd = GetForegroundWindow();
		if (lstrcmp(WinClassName, L"CabinetWClass") != 0 || StrStrW(WinText, L"Programs and Features") == NULL)
		{
			Hwnd = GetForegroundWindow();
			GetClassName(Hwnd, WinClassName, MAX_BUF);
			SendMessage(Hwnd, WM_GETTEXT, MAX_BUF, (LPARAM)WinText);
		}
		else
			break;
		Sleep(100);
	}
	
	ShowWindow(Hwnd, SW_MINIMIZE);

	HWND SysListView32 = FindNestedWindowFromClassName(Hwnd, WindowClasses, sizeof(WindowClasses) / sizeof(PWCHAR));
	if (!SysListView32)
	{
		_tprintf(L"Not found window.\n");
		goto CLEAN;
	}

	DWORD ItemCount = ListView_GetItemCount(SysListView32);
	if(ItemCount <= 0)
	{
		_tprintf(L"Item count %d.\n", ItemCount);
		goto CLEAN;
	}

	pUninstallItem pItems = (pUninstallItem)new UninstallItem[ItemCount];
	memset(pItems, 0, sizeof(UninstallItem)*ItemCount);

	HANDLE hProcess = WindowToProcess(SysListView32);
	if (!hProcess)
	{
		_tprintf(L"Window to process error.\n");
		goto CLEAN;
	}

	GetListedNames(hProcess, SysListView32, pItems, ItemCount);

	if (bList)
	{
		for (int i = 0; i < ItemCount; i++)
		{
			bool bFind = false;
			char* name = Unicode2Ansi(pItems[i].DisplayName);
			vector<ApplicationInfoA>::iterator iter = vAppInfo.begin();
			while (iter != vAppInfo.end())
			{
				if (stricmp(name, iter->strDisplayName.c_str()) == 0)
				{
					_tprintf(L"%d:%S:%S\n", i, iter->strDisplayName.c_str(), iter->strUninstallString.c_str());
					bFind = true;
					break;
				}
				++iter;
			}
			if (!bFind)
			{
				iter = vAppInfo_wow64.begin();
				while (iter != vAppInfo_wow64.end())
				{
					if (stricmp(name, iter->strDisplayName.c_str()) == 0)
					{
						_tprintf(L"%d:%S:%S\n", i, iter->strDisplayName.c_str(), iter->strUninstallString.c_str());
						break;
					}
					++iter;
				}
			}
		}
	}
	else if (nIndex >= 0)
	{
		DBClickItem(hProcess, SysListView32, nIndex);
		Sleep(100);
	}

CLEAN:
	SendMessage(Hwnd, WM_CLOSE, 0, 0);
}