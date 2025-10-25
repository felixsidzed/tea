#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	[[noreturn]] void _sys__exit(int exitCode) {
		ExitProcess(exitCode);
	}

	// TODO: ??????
	long long _sys__time(int num) {
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);

		ULONGLONG time = (((ULONGLONG)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
		return (time - 116444736000000000ull) / 10000ull;
	}

	void _sys__sleep(int milliseconds) {
		Sleep(milliseconds);
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
