#ifdef _WIN32
#include <windows.h>
#include <winnt.h>

typedef struct {
	BOOL thrown;
	const char* message;
	CONTEXT ctx;
} exception_t;

// TODO: make this thread local
static exception_t* curExc = NULL;

void _core__throw(const char* message) {
	if (!curExc)
		return;

	curExc->thrown = TRUE;
	curExc->message = message;
	RtlRestoreContext(&curExc->ctx, NULL);
}

const char* _core__pcall(void(*func)()) {
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

#else
#error "'core' is not yet available on non-windows machines"
#endif
