#ifdef _WIN32
#include <windows.h>

BOOL _mem__free(void* buf) {
	return HeapFree(GetProcessHeap(), 0, buf);
}

void* _mem__alloc(int size) {
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

#else
#error "'mem' is not yet available on non-windows machines"
#endif
