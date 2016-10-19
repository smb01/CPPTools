#include <winsock2.h>
#include <stdio.h>
#include <tchar.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")


void show_usage_and_exit(const char *prog)
{
	fprintf(stderr, "Usage: %s <target_address> <filename>\n", prog);
	exit(EXIT_FAILURE);
}

int ns_parse_address(const char *str, char* host1, int* port1) 
{
	unsigned int a, b, c, d, port;
	int n = 0, len = 0;
	char host[200] = { 0 };
	if (sscanf(str, "%199[^ :]:%u%n", host, &port, &len) == 2) {
		if (port1)
			*port1 = port;
		if (host1)
			strcpy(host1, host);
	}
	return port < 0xffff && str[len] == '\0' ? len : 0;
}

DWORD GetFileSize2(LPCTSTR pstrFile)
{
	DWORD	dwFileSize = 0;
	HANDLE	hFile = NULL;
	hFile = CreateFile(pstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return -1;
	dwFileSize = GetFileSize(hFile, NULL);
	CloseHandle(hFile);
	return dwFileSize;
}

BOOL ReadFile2(LPCTSTR lpstrFilePath, LPVOID lpBuffer, ULONG ulSize)
{
	DWORD dwRead = 0;
	DWORD dwRet = 0;
	HANDLE hFile = NULL;
	hFile = CreateFile(lpstrFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	if (!ReadFile(hFile, lpBuffer, ulSize, &dwRead, NULL))
	{
		CloseHandle(hFile);
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
}

int recv_timeout(SOCKET s, char* buf, int len, int sec)
{
	fd_set fdSet;
	struct timeval time;
	int iMode = 1;
	ioctlsocket(s, FIONBIO, (u_long FAR*)&iMode);
	time.tv_sec = sec;
	time.tv_usec = 0;
	FD_ZERO(&fdSet);
	FD_SET(s, &fdSet);
	if (select(s + 1, &fdSet, NULL, NULL, &time) <= 0)
	{
		printf("timeout\n");
		closesocket(s);
		exit(EXIT_FAILURE);
		return 0;
	}
	return recv(s, buf, len, 0);
}

int main(int argc, char** argv)
{
	struct sockaddr_in	addr;
	struct hostent* hostent = NULL;
	SOCKET s = 0;
	WSADATA wsa;

	char host[200] = { 0 };
	int port = 0;

	if (argc != 3)
		show_usage_and_exit(argv[0]);

	WSAStartup(MAKEWORD(1, 1), &wsa);

	ns_parse_address(argv[1], host, &port);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		return 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	hostent = gethostbyname(host);
	addr.sin_addr = *((struct in_addr *)hostent->h_addr);

	if (connect(s, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		printf("connect failed with error: %ld\n", WSAGetLastError());
		return 0;
	}

	char* filename = PathFindFileName(argv[2]);

	int nsend = send(s, filename, strlen(filename), 0);
	if (nsend == SOCKET_ERROR)
	{
		printf("send failed with error: %ld\n", WSAGetLastError());
		closesocket(s);
		return 0;
	}

	char ok[3] = { 0 };
	int nrecv = recv_timeout(s, ok, 2, 3);
	if (nrecv == SOCKET_ERROR)
	{
		printf("recv failed with error: %ld\n", WSAGetLastError());
		closesocket(s);
		return 0;
	}

	if (ok[0] != 'O' || ok[1] != 'K')
	{
		printf("auth failed\n");
		closesocket(s);
		return 0;
	}

	int bufLen = GetFileSize2(argv[2]);
	if (bufLen < 0)
	{
		printf("GetFileSize failed with error: %ld\n", GetLastError());
		closesocket(s);
		return 0;
	}

	char* buf = malloc(bufLen);
	if (!ReadFile2(argv[2], buf, bufLen))
	{
		printf("ReadFile failed with error: %ld\n", GetLastError());
		closesocket(s);
		return 0;
	}

	nsend = send(s, buf, bufLen, 0);
	if (nsend == SOCKET_ERROR)
	{
		printf("send failed with error: %ld\n", WSAGetLastError());
		closesocket(s);
		return 0;
	}

	printf("send ok\n");

	free(buf);
	closesocket(s);
	return 0;
}

