#ifdef _WIN32
#include <windows.h>
#include <winnt.h>

typedef struct {
	BOOL thrown;
	const char* message;
	CONTEXT ctx;
} exception_t;

extern BOOL _io__printf(const char* fmt, ...);

// TODO: make this thread local
static exception_t* curExc = NULL;

void _core__throw(const char* message) {
	if (!message) message = "[exception]";
	if (!curExc) {
		_io__printf("thread %d -> unhandled exception: %s\n", GetCurrentThreadId(), message);
		ExitThread(1);
		return;
	}

	curExc->thrown = TRUE;
	curExc->message = message;
	RtlRestoreContext(&curExc->ctx, NULL);
}

const char* _core__pcall(void(*func)()) {
	if (!func) return NULL;

	exception_t exc;
	exc.thrown = FALSE;
	exc.message = NULL;
	RtlCaptureContext(&exc.ctx);
	curExc = &exc;

	if (exc.thrown) {
		curExc = NULL;
		return exc.message;
	}

	func();

	curExc = NULL;
	return NULL;
}

BOOL _core__xpcall(void(*func)(), BOOL(*handler)(const char*)) {
	if (!func) return TRUE;

	const char* error = _core__pcall(func);
	if (handler && error)
		return handler(error);

	return !error;
}

#else
#error "'core' is not yet available on non-windows machines"
#endif
