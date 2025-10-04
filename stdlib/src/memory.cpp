#pragma runtime_checks("", off)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	void _memory__free(void* buf) {
		HeapFree(GetProcessHeap(), 0, buf);
	}

	void* _memory__alloc(size_t size) {
		return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	}

#else
#error "teastd is not yet available on non-windows machines"
#endif

#ifdef __cplusplus
}
#endif
