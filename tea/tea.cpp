#include "tea.h"

#ifndef TEA_NODEFAULTCONFIG

#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define TEA_GETCURRENTTID GetCurrentThreadId
#endif

#ifdef _DEBUG
TEA_NORETURN panic(const char* message, ...) {
	printf("thread %d panicked at 0x%llx -> ", TEA_GETCURRENTTID(), (uintptr_t)TEA_RETURNADDR());
	va_list va;
	va_start(va, message);
	vprintf(message, va);
	va_end(va);
	putchar('\n');
	__debugbreak();
}
#else
noret void panic(const char* message, ...) {
	printf("error -> ");
	va_list va;
	va_start(va, message);
	vprintf(message, va);
	va_end(va);
	putchar('\n');
	exit(1);
}
#endif

namespace tea {
	Configuration configuration = {
		.panic = panic
	};
}

#endif

