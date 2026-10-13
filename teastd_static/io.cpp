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

	void io_printch(const char ch) {
		if (!ch)
			return;
		DWORD written;
		HANDLE stdout = GetStdHandle(STD_OUTPUT_HANDLE);
		WriteConsoleA(stdout, &ch, 1, &written, nullptr);
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

	bool io_readf(const char* path, char* buf, size_t size) {
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
			return false;

		DWORD read;
		if (!ReadFile(file, buf, size, &read, NULL)) {
			CloseHandle(file);
			return false;
		}
		return true;
	}

	char* io_readline(char* buf, unsigned int size) {
		HANDLE stdin = GetStdHandle(STD_INPUT_HANDLE);
		if (stdin == INVALID_HANDLE_VALUE)
			return nullptr;

		bool heap = false;
		char* buffer = buf;
		DWORD capacity = size;

		if (!buffer) {
			capacity = 32;
			buffer = (char*)HeapAlloc(GetProcessHeap(), 0, capacity);
			if (!buffer)
				return nullptr;
			heap = true;
		}

		DWORD len = 0;
		while (true) {
			char ch;
			DWORD read;

			if (!ReadConsoleA(stdin, &ch, 1, &read, nullptr) || !read)
				break;

			if (ch == '\r') {
				ReadConsoleA(stdin, &ch, 1, &read, nullptr);
				break;
			}

			if (len + 1 >= capacity) {
				if (!heap)
					break;

				capacity += 32;
				char* newBuffer = (char*)HeapReAlloc(GetProcessHeap(), 0, buffer, capacity);
				if (!newBuffer) {
					HeapFree(GetProcessHeap(), 0, buffer);
					return nullptr;
				}
				buffer = newBuffer;
			}

			buffer[len++] = ch;
		}

		buffer[len] = '\0';
		return buffer;
	}

	// unfortunate
	extern void io_printf(const char* fmt, ...);

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

	size_t io_sizef(const char* path) {
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
			return 0;

		DWORD dwFileSizeHigh;
		DWORD dwFileSize = GetFileSize(file, &dwFileSizeHigh);
		if (dwFileSize == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
			CloseHandle(file);
			return 0; // TODO: throw an exception
		}

		CloseHandle(file);
		return dwFileSize | ((unsigned long long)dwFileSizeHigh << 32);
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
