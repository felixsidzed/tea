#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

extern BOOL _io__printf(const char* message, ...);

static BOOL initialized = FALSE;

void* _net__connect(const char* address, int port) {
	if (!initialized) {
		initialized = TRUE;
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
		return NULL;

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(address);

	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		closesocket(sock);
		return NULL;
	}

	return (void*)sock;
}


void* _net__listen(int port) {
	if (!initialized) {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		initialized = TRUE;
	}
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
		return NULL;

	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
		closesocket(sock);
		return NULL;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		closesocket(sock);
		return NULL;
	}

	if (listen(sock, 5) == SOCKET_ERROR) {
		closesocket(sock);
		return NULL;
	}

	return (void*)sock;
}

void* _net__accept(void* sock) {
	if (sock == NULL)
		return NULL;

	struct sockaddr_in clientAddr;
	int addrLen = sizeof(clientAddr);

	SOCKET client = accept((SOCKET)sock, (struct sockaddr*)&clientAddr, &addrLen);
	if (client == INVALID_SOCKET)
		return NULL;

	return (void*)client;
}

BOOL _net__send(void* socket, const char* data) {
	int result = send((SOCKET)socket, data, (int)lstrlenA(data), 0);
	return result != SOCKET_ERROR;
}

const char* _net__recv(void* socket, int bufferSize) {
	char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, bufferSize);

	int result = recv((SOCKET)socket, buffer, bufferSize, 0);
	if (result == SOCKET_ERROR) {
		HeapFree(GetProcessHeap(), 0, buffer);
		return NULL;
	}

	buffer[result] = '\0';
	return buffer;
}

void _net__close(void* socket) {
	closesocket((SOCKET)socket);
}

void _net__settimeout(void* socket, int timeout) {
	struct timeval tv;
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	setsockopt((SOCKET)socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
}

void _net__cleanup() {
	if (initialized) {
		WSACleanup();
		initialized = FALSE;
	}
}

#else
#error "'net' is not yet available on non-windows machines"
#endif
