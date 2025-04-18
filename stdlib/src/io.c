#ifdef _WIN32
#include <windows.h>

BOOL _io__print(const char* message) {
	DWORD written;
	HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutHandle != INVALID_HANDLE_VALUE && message) {
		WriteConsoleA(stdoutHandle, message, lstrlenA(message), &written, NULL);
		return TRUE;
	}
	return FALSE;
}

BOOL _io__writef(const char* path, const char* data) {
	DWORD bytesWritten;

	HANDLE hFile = CreateFileA(
		path,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile != INVALID_HANDLE_VALUE) {
		WriteFile(
			hFile,
			data,
			lstrlenA(data),
			&bytesWritten,
			NULL
		);

		CloseHandle(hFile);
		return TRUE;
	}
	return FALSE;
}

char* _io__readf(const char* path) {
    HANDLE hFile;
    DWORD dwFileSize, dwBytesRead;
    char* buffer = NULL;
    HANDLE hHeap = GetProcessHeap();

    hFile = CreateFileA(
        path,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return "[ERR] failed to open file";
    }

    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return "[ERR] failed to get file size";
    }

    buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwFileSize + 1);
    if (buffer == NULL) {
        CloseHandle(hFile);
        return "[ERR] failed to allocate buffer";
    }

    if (ReadFile(hFile, buffer, dwFileSize, &dwBytesRead, NULL)) {
        buffer[dwBytesRead] = '\0';
    } else {
        HeapFree(hHeap, 0, buffer);
        buffer = NULL;
    }

    CloseHandle(hFile);
    return buffer;
}

BOOL _io__flush() {
	HANDLE hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
	if (hConsoleInput == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!FlushConsoleInputBuffer(hConsoleInput))
		return FALSE;
	
	return TRUE;
}

char* _io__readline() {
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	const DWORD chunkSize = 128;
	CHAR* buffer = (CHAR*)HeapAlloc(GetProcessHeap(), 0, chunkSize);
	if (!buffer) return "[ERR] failed to allocate";

	DWORD capacity = chunkSize;
	DWORD length = 0;

	while (1) {
		CHAR ch;
		DWORD read;

		if (!ReadConsoleA(hStdin, &ch, 1, &read, NULL) || read == 0)
			break;

		if (ch == '\r') {
			ReadConsoleA(hStdin, &ch, 1, &read, NULL);
			break;
		}

		if (length + 1 >= capacity) {
			capacity += chunkSize;
			CHAR* newBuffer = (CHAR*)HeapReAlloc(GetProcessHeap(), 0, buffer, capacity);
			if (!newBuffer) {
				HeapFree(GetProcessHeap(), 0, buffer);
				return "[ERR] failed to reallocate";
			}
			buffer = newBuffer;
		}

		buffer[length++] = ch;
	}

	buffer[length] = '\0';

	return buffer;
}

#else
#error "'io' is not yet available on non-windows machines"
#endif
