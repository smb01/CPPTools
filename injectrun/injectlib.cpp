#include  "injectlib.h"


HANDLE _CreateRemoteThread(HANDLE hProcess,
						   LPSECURITY_ATTRIBUTES  lpThreadAttributes,
						   DWORD                  dwStackSize, 
						   LPTHREAD_START_ROUTINE lpStartAddress,
						   LPVOID                 lpParameter,
						   DWORD                  dwCreationFlags,
						   LPDWORD                lpThreadId)
{
	NTSTATUS               Status;
	HANDLE                 hThread;
	NTCREATETHREADEXBUFFER ntBuffer;
	DWORD                  dw0, dw1,  MyOSMajorVersion, MyOSMinorVersion;
	OSVERSIONINFO		   osvi;
	BOOL				   MyOSWinNT, MyOSWinNT3_2003, MyOSWinVista_7, MyOSWinVista_8;
	HMODULE				   hKernel32, hNTDLL;
	CREATEREMOTETHREAD	   MyCreateRemoteThread;
	GETTHREADID            MyGetThreadId;
	RTLNTSTATUSTODOSERROR  MyRtlNtStatusToDosError;
	NTCREATETHREADEX       MyNtCreateThreadEx;

	// Get Windows version
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&osvi))
		return NULL;

	// Save version data in global variables
	MyOSMajorVersion = osvi.dwMajorVersion;
	MyOSMinorVersion = osvi.dwMinorVersion;
	MyOSWinNT = osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;
	MyOSWinNT3_2003 = (MyOSWinNT && MyOSMajorVersion >= 3 && MyOSMajorVersion <= 5) ? TRUE : FALSE; // Win 3.1 to 2003
	MyOSWinVista_7 = (MyOSWinNT && MyOSMajorVersion == 6 && MyOSMinorVersion <= 1) ? TRUE : FALSE; // Win Vista to 7
	MyOSWinVista_8 = (MyOSWinNT && MyOSMajorVersion == 6 && MyOSMinorVersion >= 2) ? TRUE : FALSE; // 8 to ?

	/***** Win NT *****/
	if (MyOSWinNT)
	{
		if (!(hKernel32 = LoadLibrary("Kernel32.dll")))
			return NULL;
		if (!(hNTDLL = LoadLibrary("NTDLL.DLL")))
			return NULL;

		// Win NT all versions
		if (!(MyCreateRemoteThread = (CREATEREMOTETHREAD)GetProcAddress(hKernel32, "CreateRemoteThread")))
			return NULL;
		if (!(MyRtlNtStatusToDosError = (RTLNTSTATUSTODOSERROR)GetProcAddress(hNTDLL, "RtlNtStatusToDosError")))
			return NULL;

		// Win 2003 or later
		if ((MyOSMajorVersion == 5 && MyOSMinorVersion >= 2) || MyOSMajorVersion == 6)
		{
			if (!(MyGetThreadId = (GETTHREADID)GetProcAddress(hKernel32, "GetThreadId")))
				return NULL;
		}

		// Win Vista or later
		if (MyOSMajorVersion >= 6)
		{
			if (!(MyNtCreateThreadEx = (NTCREATETHREADEX)GetProcAddress(hNTDLL, "NtCreateThreadEx")))
				return NULL;
		}
	}//WinNT

    // Win NT 3.1 to 2003
    if (MyOSWinNT3_2003)
	{
		return MyCreateRemoteThread(hProcess,
			lpThreadAttributes,
			dwStackSize,
			lpStartAddress,
			lpParameter,
			dwCreationFlags,
			lpThreadId);
	}
    // Win Vista or later
    else if (MyOSWinVista_7)
	{
		// Setup and initialize the buffer
        memset(&ntBuffer, 0, sizeof(NTCREATETHREADEXBUFFER));
        dw0 = 0;
        dw1 = 0;
        ntBuffer.Size = sizeof(NTCREATETHREADEXBUFFER);
        ntBuffer.Unknown1 = 0x10003;
        ntBuffer.Unknown2 = 0x8;
        ntBuffer.Unknown3 = &dw1;
        ntBuffer.Unknown4 = 0;
        ntBuffer.Unknown5 = 0x10004;
        ntBuffer.Unknown6 = 4;
        ntBuffer.Unknown7 = &dw0;
        ntBuffer.Unknown8 = 0;
		Status = MyNtCreateThreadEx(&hThread, 0x1FFFFF, NULL, hProcess, lpStartAddress, lpParameter, FALSE, NULL, NULL, NULL, NULL); 
        if (!NT_SUCCESS(Status))
		{
            SetLastError(MyRtlNtStatusToDosError(Status));
            return NULL;
		}
		if (lpThreadId)
			*lpThreadId = MyGetThreadId(hThread);
		return hThread;
	}
	// Win8 to win10~?
    else
	{
		return MyCreateRemoteThread(hProcess,
			lpThreadAttributes,
			dwStackSize,
			lpStartAddress,
			lpParameter,
			dwCreationFlags,
			lpThreadId);
	}
}