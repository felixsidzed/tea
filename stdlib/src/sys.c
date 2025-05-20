#ifdef _WIN32
#include <windows.h>

#define EPOCH_DIFFERENCE 116444736000000000ULL

void _sys__exit(int code) {
	ExitProcess(code);
}

long _sys__time() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);

	ULONGLONG time = (((ULONGLONG)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;

	return (time - EPOCH_DIFFERENCE) / 10000000ULL;
}

void _sys__sleep(int ms) {
	Sleep(ms);
}

#else
#error "'sys' is not yet available on non-windows machines"
#endif
