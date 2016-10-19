#ifndef __INJECTLIB_H__
#define __INJECTLIB_H__

#include <windows.h>
#include <tlhelp32.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
	PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

// Buffer argument passed to NtCreateThreadEx function
typedef struct _NTCREATETHREADEXBUFFER
 {
   ULONG  Size;
   ULONG  Unknown1;
   ULONG  Unknown2;
   PULONG Unknown3;
   ULONG  Unknown4;
   ULONG  Unknown5;
   ULONG  Unknown6;
   PULONG Unknown7;
   ULONG  Unknown8;
 } NTCREATETHREADEXBUFFER;

// System functions loaded dinamically
typedef DWORD (WINAPI *GETTHREADID)(HANDLE);
typedef HANDLE (WINAPI *CREATEREMOTETHREAD)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef NTSTATUS (NTAPI *NTCREATETHREADEX)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, HANDLE, LPTHREAD_START_ROUTINE, LPVOID, ULONG, ULONG, ULONG, ULONG, LPVOID);
typedef ULONG (NTAPI *RTLNTSTATUSTODOSERROR)(NTSTATUS);

HANDLE _CreateRemoteThread(HANDLE hProcess,
						   LPSECURITY_ATTRIBUTES  lpThreadAttributes,
						   DWORD                  dwStackSize, 
						   LPTHREAD_START_ROUTINE lpStartAddress,
						   LPVOID                 lpParameter,
						   DWORD                  dwCreationFlags,
						   LPDWORD                lpThreadId);


#endif  // __INJECTLIB_H__
