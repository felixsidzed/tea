#ifdef _WIN32
#include <windows.h>

BOOL _mem__free(void* buf) {
	return HeapFree(GetProcessHeap(), 0, buf);
}

#else
#error "'mem' is not yet available on non-windows machines"
#endif
