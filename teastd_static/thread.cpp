#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	void* thread_spawn(void* f) {
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

	void thread_join(void* thread) {
		WaitForSingleObject(thread, INFINITE);
	}

	void thread_close(void* thread) {
		CloseHandle(thread);
	}

	[[noreturn]] void thread_exit(int exitCode) {
		ExitThread(exitCode);
	}

	void* thread_self() {
		return GetCurrentThread();
	}

	extern "C" {
		#pragma section(".tls$0", read, write)
		__declspec(allocate(".tls$0")) char _tls_start;
		#pragma section(".tls$ZZZ", read, write)
		__declspec(allocate(".tls$ZZZ")) char _tls_end;
	}

	ULONG _tls_index = 0;
	
	#pragma const_seg(".rdata$T")
	extern "C" const IMAGE_TLS_DIRECTORY _tls_used = {
		(ULONG_PTR)&_tls_start, (ULONG_PTR)&_tls_end,
		(ULONG_PTR)&_tls_index,
		0,
	};
	#pragma const_seg()

	#ifdef _WIN64
		#pragma comment(linker, "/include:_tls_used")
	#else 
		#pragma comment(linker, "/include:__tls_used")
	#endif

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
