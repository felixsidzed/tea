#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>

	SOCKET socket_connect(const char* host, unsigned short port) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa))
			return NULL;

		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET)
			return NULL;

		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
			closesocket(s);
			return NULL;
		}

		if (connect(s, (struct sockaddr*)&addr, sizeof(addr))) {
			closesocket(s);
			return NULL;
		}

		return s;
	}

	SOCKET socket_listen(unsigned short port) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa))
			return NULL;

		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET)
			return NULL;

		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(s, (struct sockaddr*)&addr, sizeof(addr))) {
			closesocket(s);
			return NULL;
		}

		if (listen(s, SOMAXCONN)) {
			closesocket(s);
			return NULL;
		}

		return s;
	}

	SOCKET socket_accept(SOCKET sock) {
		SOCKET client = accept(sock, NULL, NULL);
		if (client == INVALID_SOCKET)
			return 0;

		return client;
	}

	int socket_send(SOCKET sock, const char* data, int size) {
		return send(sock, data, size, 0);
	}

	int socket_recv(SOCKET sock, char* buf, int size) {
		return recv(sock, buf, size, 0);
	}

	void socket_close(SOCKET sock) {
		if (!sock)
			return;

		closesocket(sock);
		WSACleanup();
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
