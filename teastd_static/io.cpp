#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	void io_print(const char* message) {
		if (!message)
			return;
		DWORD written;
		HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
		WriteConsoleA(stdout, message, lstrlenA(message), &written, nullptr);
	}

	bool io_writef(const char* path, const char* data, int size) {
		HANDLE file = CreateFileA(
			path,
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (file != INVALID_HANDLE_VALUE) {
			DWORD written;
			WriteFile(
				file,
				data, size,
				&written, NULL
			);

			CloseHandle(file);
			return true;
		}
		return false;
	}

	char* io_readf(const char* path) {
		HANDLE file = CreateFileA(
			path,
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (file == INVALID_HANDLE_VALUE)
			return NULL;

		DWORD dwFileSize = GetFileSize(file, NULL);
		if (dwFileSize == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
			CloseHandle(file);
			return nullptr; // TODO: throw an exception
		}

		char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize + 1);
		if (buffer == NULL) {
			CloseHandle(file);
			return nullptr; // TODO: throw an exception
		}

		DWORD read;
		if (ReadFile(file, buffer, dwFileSize, &read, NULL))
			buffer[read] = '\0';
		else {
			HeapFree(GetProcessHeap(), 0, buffer);
			buffer = NULL;
		}

		CloseHandle(file);
		return buffer;
	}

	char* io_readline() {
		HANDLE stdin = GetStdHandle(STD_INPUT_HANDLE);

		const DWORD CHUNK_SIZE = 128;
		char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, CHUNK_SIZE);
		if (!buffer)
			return nullptr; // TODO: throw an exception

		DWORD len = 0;
		DWORD capacity = CHUNK_SIZE;

		while (true) {
			char ch;
			DWORD read;
			if (!ReadConsoleA(stdin, &ch, 1, &read, NULL) || read == 0)
				break;

			if (ch == '\r') {
				ReadConsoleA(stdin, &ch, 1, &read, NULL);
				break;
			}

			if (len + 1 >= capacity) {
				capacity += CHUNK_SIZE;
				char* newBuffer = (char*)HeapReAlloc(GetProcessHeap(), 0, buffer, capacity);
				if (!newBuffer) {
					HeapFree(GetProcessHeap(), 0, buffer);
					return nullptr; // TODO: throw an exception
				}
				buffer = newBuffer;
			}

			buffer[len++] = ch;
		}

		buffer[len] = '\0';
		return buffer;
	}

	// modified
	// TODO: support every format
	// from: nanobyte-dev/nanobyte_os
	#define PRINTF_STATE_NORMAL         0
	#define PRINTF_STATE_LENGTH         1
	#define PRINTF_STATE_LENGTH_SHORT   2
	#define PRINTF_STATE_LENGTH_LONG    3
	#define PRINTF_STATE_SPEC           4

	#define PRINTF_LENGTH_DEFAULT       0
	#define PRINTF_LENGTH_SHORT_SHORT   1
	#define PRINTF_LENGTH_SHORT         2
	#define PRINTF_LENGTH_LONG          3
	#define PRINTF_LENGTH_LONG_LONG     4

	static const char hexChars[] = "0123456789abcdef";

	static void printunsigned(unsigned long long number, int radix) {
		HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);

		int pos = 0;
		char buffer[32];

		do {
			uintptr_t rem = number % radix;
			number /= radix;
			buffer[pos++] = hexChars[rem];
		} while (number > 0);

		DWORD written;
		while (--pos >= 0)
			WriteConsoleA(stdout, &buffer[pos], 1, &written, nullptr);
	}

	static void printsigned(long long number, int radix) {
		if (number < 0) {
			DWORD written;
			HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
			WriteConsoleA(stdout, "-", 1, &written, nullptr);
			printunsigned(-number, radix);
		} else
			printunsigned(number, radix);
	}

	void io_printf(const char* fmt, ...) {
		va_list va;
		va_start(va, fmt);

		int radix = 10;
		bool sign = false;
		bool number = false;
		int state = PRINTF_STATE_NORMAL;
		int length = PRINTF_LENGTH_DEFAULT;

		DWORD written;
		HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);

		while (*fmt) {
			switch (state) {
			case PRINTF_STATE_NORMAL:
				switch (*fmt) {
				case '%':
					state = PRINTF_STATE_LENGTH;
					break;
				default:
					WriteConsoleA(stdout, fmt, 1, &written, nullptr);
					break;
				}
				break;

			case PRINTF_STATE_LENGTH:
				switch (*fmt) {
				case 'h':
					length = PRINTF_LENGTH_SHORT;
					state = PRINTF_STATE_LENGTH_SHORT;
					break;
				case 'l':
					length = PRINTF_LENGTH_LONG;
					state = PRINTF_STATE_LENGTH_LONG;
					break;
				default:
					goto PRINTF_STATE_SPEC_;
				}
				break;

			case PRINTF_STATE_LENGTH_SHORT:
				if (*fmt == 'h') {
					length = PRINTF_LENGTH_SHORT_SHORT;
					state = PRINTF_STATE_SPEC;
				} else
					goto PRINTF_STATE_SPEC_;
				break;

			case PRINTF_STATE_LENGTH_LONG:
				if (*fmt == 'l') {
					state = PRINTF_STATE_SPEC;
					length = PRINTF_LENGTH_LONG_LONG;
				} else
					goto PRINTF_STATE_SPEC_;
				break;

			case PRINTF_STATE_SPEC:
			PRINTF_STATE_SPEC_:
				switch (*fmt) {
				case 'c': {
					char arg = (char)va_arg(va, int);
					WriteConsoleA(stdout, &arg, 1, &written, nullptr);
				} break;

				case 's': {
					const char* arg = (const char*)va_arg(va, const char*);
					WriteConsoleA(stdout, arg, lstrlenA(arg), &written, nullptr);
				} break;

				case '%':
					WriteConsoleA(stdout, "%", 1, &written, nullptr);
					break;

				case 'd':
				case 'i':
					radix = 10;
					sign = true;
					number = true;
					break;

				case 'u':
					radix = 10;
					sign = false;
					number = true;
					break;

				case 'X':
				case 'x':
				case 'p':
					radix = 16;
					sign = false;
					number = true;
					break;

				case 'o':
					radix = 8;
					sign = false;
					number = true;
					break;

				default:
					break;
				}

				if (number) {
					if (sign) {
						switch (length) {
						case PRINTF_LENGTH_SHORT_SHORT:
						case PRINTF_LENGTH_SHORT:
						case PRINTF_LENGTH_DEFAULT:
							printsigned(va_arg(va, int), radix);
							break;

						case PRINTF_LENGTH_LONG:
							printsigned(va_arg(va, long), radix);
							break;

						case PRINTF_LENGTH_LONG_LONG:
							printsigned(va_arg(va, long long), radix);
							break;
						}
					} else {
						switch (length) {
						case PRINTF_LENGTH_SHORT_SHORT:
						case PRINTF_LENGTH_SHORT:
						case PRINTF_LENGTH_DEFAULT:
							printsigned(va_arg(va, unsigned int), radix);
							break;

						case PRINTF_LENGTH_LONG:
							printsigned(va_arg(va, unsigned long), radix);
							break;

						case PRINTF_LENGTH_LONG_LONG:
							printsigned(va_arg(va, unsigned long long), radix);
							break;
						}
					}
				}

				state = PRINTF_STATE_NORMAL;
				length = PRINTF_LENGTH_DEFAULT;
				radix = 10;
				sign = false;
				number = false;
				break;
			}

			fmt++;
		}

		va_end(va);
	}

	bool io_existsf(const char* path) {
		DWORD dwAttrib = GetFileAttributesA(path);
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool io_delf(const char* path) {
		return DeleteFileA(path);
	}

	bool io_mkdir(const char* path) {
		return CreateDirectoryA(path, nullptr);
	}

	bool io_rmdir(const char* path) {
		return RemoveDirectoryA(path);
	}

	bool io_appendf(const char* path, const char* data, int size) {
		HANDLE file = CreateFileA(
			path,
			FILE_APPEND_DATA,
			FILE_SHARE_READ,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (file != INVALID_HANDLE_VALUE) {
			DWORD written;
			WriteFile(
				file,
				data, size,
				&written, NULL
			);

			CloseHandle(file);
			return true;
		}
		return false;
	}

	extern void* memory_alloc(size_t size);

	const char* io_tempdir() {
		char tempPath[MAX_PATH];

		if (!GetTempPathA(MAX_PATH, tempPath))
			return nullptr;

		char* result = (char*)memory_alloc(MAX_PATH);
		if (!GetTempFileNameA(tempPath, "", 0, result))
			return nullptr;

		DeleteFileA(result);

		if (!CreateDirectoryA(result, nullptr))
			return nullptr;

		return result;
	}

	const char* io_tempfile() {
		char tempPath[MAX_PATH];

		if (!GetTempPathA(MAX_PATH, tempPath))
			return nullptr;

		char* result = (char*)memory_alloc(MAX_PATH);
		if (!GetTempFileNameA(tempPath, "", 0, result))
			return nullptr;

		return result;
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
