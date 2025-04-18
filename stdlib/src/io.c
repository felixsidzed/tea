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

	buffer = (char*)LocalAlloc(LPTR, dwFileSize + 1);
	if (buffer == NULL) {
		CloseHandle(hFile);
		return "[ERR] failed to allocate buffer";
	}

	if (ReadFile(hFile, buffer, dwFileSize, &dwBytesRead, NULL)) {
		buffer[dwBytesRead] = '\0';
	} else {
		LocalFree(buffer);
		buffer = NULL;
	}

	CloseHandle(hFile);
	return buffer;
}

#else
#error "'io' is not yet available on non-windows machines"
#endif
