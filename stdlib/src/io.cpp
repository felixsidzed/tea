#ifdef _WIN32
#include <windows.h>

extern "C" void _io__print(const char* message) {
	DWORD written;
	HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutHandle != INVALID_HANDLE_VALUE && message) {
		WriteConsoleA(stdoutHandle, message, lstrlenA(message), &written, nullptr);
	}
}
#else
#error "stdio is not yet available on non-windows machines"
#endif
