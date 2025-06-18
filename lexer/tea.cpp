#include "tea.h"

#if !defined(TEA_NODEFAULTCONFIG)

#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

#define noret [[noreturn]]
#define getCurrentTid GetCurrentThreadId
#define getRetAddr() _ReturnAddress()
#else
#define noret __declspec((noreturn))
#define getRetAddr() __builtin_return_address(0)
#endif

noret void panic(const char* message, ...) {
	printf("thread %d panicked at 0x%llx -> ", getCurrentTid(), (uintptr_t)getRetAddr());
	va_list va;
	va_start(va, message);
	vprintf(message, va);
	va_end(va);
	putchar('\n');
	exit(1);
}

namespace tea {
	Configuration configuration = {
		.panic = panic
	};
}

#endif

