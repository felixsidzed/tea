#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	void* _thread__spawn(void* f) {
		DWORD tid;
		HANDLE handle;

		handle = CreateThread(
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)f,
			NULL,
			0,
			&tid
		);

		if (handle == NULL)
			return INVALID_HANDLE_VALUE;

		return handle;
	}

	void _thread__join(void* thread) {
		WaitForSingleObject(thread, INFINITE);
	}

	void _thread__close(void* thread) {
		CloseHandle(thread);
	}

	[[noreturn]] void _thread__exit(int exitCode) {
		ExitThread(exitCode);
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
