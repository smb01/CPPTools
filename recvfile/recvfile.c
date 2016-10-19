#include <winsock2.h>
#include <stdio.h>
#include <tchar.h>
#pragma comment(lib, "ws2_32.lib")


void show_usage_and_exit(const char *prog)
{
	fprintf(stderr, "Usage: %s <port>\n", prog);
	exit(EXIT_FAILURE);
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
	char buf[8096];
	int listenPort;
	SOCKET server;
	WSADATA wsa;
	
	if (argc != 2)
		show_usage_and_exit(argv[0]);

	WSAStartup(MAKEWORD(2, 2), &wsa);

	listenPort = atoi(argv[1]);

	server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		return FALSE;
	}

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(listenPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	if (bind(server, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("bind failed with error: %ld\n", WSAGetLastError());
		closesocket(server);
		return FALSE;
	}

	if (listen(server, 5) == SOCKET_ERROR)
	{
		printf("listen failed with error: %ld\n", WSAGetLastError());
		closesocket(server);
		return FALSE;
	}

	while (1)
	{
		SOCKET client = accept(server, NULL, NULL);
		if (client == INVALID_SOCKET)
		{
			printf("accept failed with error: %ld\n", WSAGetLastError());
			continue;
		}

		char filename[1024] = { 0 };
		int nRecv = recv_timeout(client, filename, sizeof(filename), 3);
		if (nRecv <= 0)
		{
			printf("recv failed with error: %ld\n", WSAGetLastError());
			closesocket(client);
			continue;
		}

		filename[nRecv] = '\0';

		printf("recv filename:%s\n", filename);

		char* ok = "OK";
		int nSend = send(client, ok, strlen(ok), 0);
		if (nSend <= 0)
		{
			printf("send failed with error: %ld\n", WSAGetLastError());
			closesocket(client);
			continue;
		}

		DWORD dwWritten = 0;
		HANDLE hFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			printf("CreateFile failed with error: %ld\n", GetLastError());
			closesocket(client);
			continue;
		}

		nRecv = recv_timeout(client, buf, sizeof(buf), 3);
		if (nRecv <= 0)
		{
			printf("recv failed with error: %ld\n", WSAGetLastError());
			closesocket(client);
			CloseHandle(hFile);
			DeleteFile(filename);
			continue;
		}

		while (nRecv > 0)
		{
			WriteFile(hFile, buf, nRecv, &dwWritten, NULL);
			nRecv = recv_timeout(client, buf, sizeof(buf), 3);
		}

		printf("recv finished\n");

		closesocket(client);
		CloseHandle(hFile);
	}
	
	closesocket(server);

	return 0;
}

