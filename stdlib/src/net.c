#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

static BOOL initialized = FALSE;

int net_init() {
	initialized = TRUE;
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void* _net__connect(const char* address, int port) {
	if (!initialized)
		net_init();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return NULL;
    }

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

BOOL _net__send(void* socket, const char* data) {
    SOCKET sock = (SOCKET)socket;
    int result = send(sock, data, (int)lstrlenA(data), 0);
    return result != SOCKET_ERROR;
}

#else
#error "'net' is not yet available on non-windows machines"
#endif
