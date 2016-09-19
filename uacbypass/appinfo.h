#include <Windows.h>
#include <string>
#include <vector> 

using namespace std;

struct ApplicationInfoA
{
	string strName;
	string strDisplayName;
	string strPublisher;
	string strVersion;
	string strDisplayVersion;
	string strInstallLocation;
	string strUninstallString;
};

int _GetAppInfoA_(HKEY hKey, LPSTR lpszAppName, LPCSTR lpszKeyValueName, string& strKeyValue);
int GetAllInstalledAppInfoA(LPCSTR lpszSubKey, vector<ApplicationInfoA>& vAppInfo);
