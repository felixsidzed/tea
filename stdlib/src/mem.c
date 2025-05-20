#ifdef _WIN32
#include <windows.h>

BOOL _mem__free(void* buf) {
	return HeapFree(GetProcessHeap(), 0, buf);
}

void* _mem__alloc(int size) {
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void* _mem__copy(void* dest, const void* src, int n) {
	const char* f = src;
	char* t = dest;

	while (n-- > 0)
		*t++ = *f++;
	return dest;
}

#else
#error "'mem' is not yet available on non-windows machines"
#endif
