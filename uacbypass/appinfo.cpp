#include "appinfo.h"


int _GetAppInfoA_(HKEY hKey, LPSTR lpszAppName, LPCSTR lpszKeyValueName, string& strKeyValue)
{
	int ret;
	HKEY hInstallAppKey;
	ret = RegOpenKeyExA(hKey, lpszAppName, 0, KEY_READ, &hInstallAppKey);
	if (ret != ERROR_SUCCESS)
		return -1;

	DWORD dwKeyValueType = REG_SZ;
	DWORD dwKeyValueDataSize = 0;
	ret = RegQueryValueExA(hInstallAppKey, lpszKeyValueName, NULL, &dwKeyValueType, NULL, &dwKeyValueDataSize);
	if (ret == ERROR_FILE_NOT_FOUND)
	{
		RegCloseKey(hInstallAppKey);
		return 0;
	}
	else if (ret != ERROR_SUCCESS)
	{
		RegCloseKey(hInstallAppKey);
		return -1;
	}

	if (dwKeyValueType != REG_SZ && dwKeyValueType != REG_EXPAND_SZ)
	{
		RegCloseKey(hInstallAppKey);
		return 0;
	}
	LPSTR lpszKeyValueData = new char[dwKeyValueDataSize + 1];
	memset(lpszKeyValueData, 0, dwKeyValueDataSize + 1);
	ret = RegQueryValueExA(hInstallAppKey, lpszKeyValueName, NULL, &dwKeyValueType, (LPBYTE)lpszKeyValueData, &dwKeyValueDataSize);
	if (ret != ERROR_SUCCESS)
	{
		delete[] lpszKeyValueData;
		RegCloseKey(hInstallAppKey);
		return -1;
	}
	strKeyValue = lpszKeyValueData;
	delete[] lpszKeyValueData;
	RegCloseKey(hInstallAppKey);
	return 0;
}

int GetAllInstalledAppInfoA(LPCSTR lpszSubKey, vector<ApplicationInfoA>& vAppInfo)
{
	int ret = 0;
	HKEY hKey = NULL;
	ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, lpszSubKey, 0, KEY_READ, &hKey);
	if (ret != ERROR_SUCCESS)
		return -1;

	DWORD dwSubKeysCnt;
	DWORD dwMaxSubKeyNameLen;
	DWORD dwKeyValueCnt;
	DWORD dwMaxKeyValueNameLen;
	DWORD dwMaxKeyValueDataLen;

	ret = RegQueryInfoKey(hKey, NULL, NULL, NULL, &dwSubKeysCnt, &dwMaxSubKeyNameLen, NULL, &dwKeyValueCnt, &dwMaxKeyValueNameLen, &dwMaxKeyValueDataLen, NULL, NULL);
	if (ret != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	DWORD dwIndex;
	LPSTR lpszSubKeyName = new char[dwMaxSubKeyNameLen + 1];
	DWORD dwNameLen = dwMaxSubKeyNameLen + 1;

	for (dwIndex = 0; dwIndex < dwSubKeysCnt; ++dwIndex)
	{
		dwNameLen = dwMaxSubKeyNameLen + 1;
		memset(lpszSubKeyName, 0, dwMaxSubKeyNameLen + 1);

		ret = RegEnumKeyExA(hKey, dwIndex, lpszSubKeyName, &dwNameLen, NULL, NULL, NULL, NULL);
		if (ret != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			delete[] lpszSubKeyName;
			return -1;
		}

		ApplicationInfoA appInfo;
		appInfo.strName = lpszSubKeyName;
		_GetAppInfoA_(hKey, lpszSubKeyName, "DisplayName", appInfo.strDisplayName);
		_GetAppInfoA_(hKey, lpszSubKeyName, "Publisher", appInfo.strPublisher);
		_GetAppInfoA_(hKey, lpszSubKeyName, "Version", appInfo.strVersion);
		_GetAppInfoA_(hKey, lpszSubKeyName, "DisplayVersion", appInfo.strDisplayVersion);
		_GetAppInfoA_(hKey, lpszSubKeyName, "InstallLocation", appInfo.strInstallLocation);
		_GetAppInfoA_(hKey, lpszSubKeyName, "UninstallString", appInfo.strUninstallString);
		vAppInfo.push_back(appInfo);
	}

	delete[] lpszSubKeyName;
	RegCloseKey(hKey);
	return 0;
}