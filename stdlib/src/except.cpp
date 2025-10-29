#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	typedef struct {
		bool thrown;
		const char* message;
		CONTEXT ctx;
	} exception_t;

	static __declspec(thread) exception_t* curExc = nullptr ;

	extern void _io__printf(const char* fmt, ...);

	[[noreturn]] void _except__throw(const char* message) {
		if (!message) message = "[exception]";
		if (!curExc) {
			_io__printf("thread %d -> unhandled exception: %s\n", GetCurrentThreadId(), message);
			ExitThread(1);
		}

		curExc->thrown = true;
		curExc->message = message;
		RtlRestoreContext(&curExc->ctx, nullptr);
	}

	const char* _except__pcall(void(*func)()) {
		if (!func)
			return nullptr;

		exception_t* exc = (exception_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(exception_t));
		if (!exc)
			return 0;

		exc->thrown = false;
		exc->message = nullptr;
		curExc = exc;

		RtlCaptureContext(&exc->ctx);
		if (exc->thrown) {
			const char* message = exc->message;
			curExc = nullptr;
			HeapFree(GetProcessHeap(), 0, exc);
			return message;
		}

		func();

		curExc = nullptr;
		HeapFree(GetProcessHeap(), 0, exc);
		return nullptr;
	}

	BOOL _except__xpcall(void(*func)(), BOOL(*handler)(const char*)) {
		if (!func)
			return true;

		const char* error = _except__pcall(func);
		if (handler && error)
			return handler(error);

		return !error;
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
