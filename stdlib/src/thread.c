#ifdef _WIN32
#include <windows.h>

void* _thread__create(void* f) {
	DWORD tid;
    HANDLE handle;

    handle = CreateThread(
        NULL,
        0,
        f,
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

#else
#error "'thread' is not yet available on non-windows machines"
#endif
