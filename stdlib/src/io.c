#ifdef _WIN32
#include <windows.h>

static const char DIGITS[] = "0123456789abcdef";
static const char DIGITS_UPPER[] = "0123456789ABCDEF";

int _itoa_unsigned(unsigned int value, char *buffer, int base, BOOL upper) {
	const char *d = upper ? DIGITS_UPPER : DIGITS;

	char temp[32];
	int i = 0;

	if (value == 0)
		temp[i++] = '0';

	while (value > 0) {
		temp[i++] = d[value % base];
		value /= base;
	}

	int j = 0;
	while (i > 0) {
		buffer[j++] = temp[--i];
	}
	buffer[j] = '\0';

	return j;
}

int _itoa_signed(int value, char *buffer) {
	unsigned int u = (value < 0) ? -value : value;
	int i = 0;
	if (value < 0) {
		buffer[i++] = '-';
	}
	i += _itoa_unsigned(u, buffer + i, 10, FALSE);
	return i;
}

BOOL _io__print(const char* message) {
	DWORD written;
	HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	int len = lstrlenA(message);
	if (stdout != INVALID_HANDLE_VALUE && message) {
		WriteConsoleA(stdout, message, len, &written, NULL);
		return written == len;
	}
	return FALSE;
}

BOOL _io__printf(const char* fmt, ...) {
	DWORD written;
	BOOL success = TRUE;
	HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdout == INVALID_HANDLE_VALUE) return FALSE;
	
	va_list va;
	va_start(va, fmt);

	const char* c = fmt;
	while (c && *c) {
		if (*c == '%') {
			c++;
			char buf[64];

			switch (*c) {
				case 'd':
				case 'i': {
					int num = va_arg(va, int);
					_itoa_signed(num, buf);
					int len = lstrlenA(buf);
					success = success && WriteConsoleA(stdout, buf, len, &written, NULL) && written == len;
				} break;

				case 'u': {
					unsigned int num = va_arg(va, unsigned int);
					_itoa_unsigned(num, buf, 10, FALSE);
					int len = lstrlenA(buf);
					success = success && WriteConsoleA(stdout, buf, len, &written, NULL) && written == len;
				} break;

				case 'x': {
					unsigned int num = va_arg(va, unsigned int);
					_itoa_unsigned(num, buf, 16, FALSE);
					int len = lstrlenA(buf);
					success = success && WriteConsoleA(stdout, buf, len, &written, NULL) && written == len;
				} break;

				case 'X': {
					unsigned int num = va_arg(va, unsigned int);
					_itoa_unsigned(num, buf, 16, TRUE);
					int len = lstrlenA(buf);
					success = success && WriteConsoleA(stdout, buf, len, &written, NULL) && written == len;
				} break;

				case 'p': {
					void* ptr = va_arg(va, void*);
					unsigned __int64 addr = (unsigned __int64)ptr;
					buf[0] = '0';
					buf[1] = 'x';
					_itoa_unsigned((unsigned int)((addr >> 32) & 0xFFFFFFFF), buf + 2, 16, FALSE);
					int hi_len = lstrlenA(buf + 2);
					_itoa_unsigned((unsigned int)(addr & 0xFFFFFFFF), buf + 2 + hi_len, 16, FALSE);
					int len = lstrlenA(buf);
					success = success && WriteConsoleA(stdout, buf, len, &written, NULL) && written == len;
				} break;

				case 'c': {
					char ch = (char)va_arg(va, int);
					success = success && WriteConsoleA(stdout, &ch, 1, &written, NULL) && written == 1;
				} break;

				case 's': {
					char* str = va_arg(va, char*);
					if (!str) str = "(null)";
					int len = lstrlenA(str);
					success = success && WriteConsoleA(stdout, str, len, &written, NULL) && written == len;
				} break;

				default:
					success = success && WriteConsoleA(stdout, c, 1, &written, NULL) && written == 1;
					break;
			}
		} else {
			success = success && WriteConsoleA(stdout, c, 1, &written, NULL) && written == 1;
		}

		c++;
	}

	va_end(va);
	return success;
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
        return "Failed to open file!";
    }

    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return "Failed to read file!";
    }

    buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwFileSize + 1);
    if (buffer == NULL) {
        CloseHandle(hFile);
        return "ERR_MEM";
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
	if (!buffer) return "ERR_MEM";

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
				return "ERR_MEM";
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
