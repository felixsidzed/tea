#include "common/tea.h"

#include "frontend/lexer/Lexer.h"

#ifndef TEA_NODEFAULTCONFIG
	#include <format>
	#include <cstdio>
	#include <cstdlib>
	#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	#define TEA_CURRENTTID GetCurrentThreadId()
	#endif

	#ifdef _DEBUG
	TEA_NORETURN static panic(const char* message, ...) {
		va_list va;
		va_start(va, message);

		va_list vacopy;
		va_copy(vacopy, va);
		int size = vsnprintf(nullptr, 0, message, vacopy);
		va_end(vacopy);

		if (size < 0) {
			va_end(va);
			throw std::runtime_error("Error in panic handler");
		}

		char* buffer = new char[size + 1];
		vsnprintf(buffer, size + 1, message, va);
		va_end(va);

		throw std::runtime_error(std::format("thread {} panicked at {:#x} -> {}", TEA_CURRENTTID, (uintptr_t)TEA_RETURNADDR, buffer));
		delete[] buffer;
	}
	#else
	TEA_NORETURN static panic(const char* message, ...) {
		va_list va;
		va_start(va, message);

		va_list vacopy;
		va_copy(vacopy, va);
		int size = vsnprintf(nullptr, 0, message, vacopy);
		va_end(vacopy);

		if (size < 0) {
			va_end(va);
			throw std::runtime_error("Error in panic handler");
		}

		char* buffer = new char[size + 1];
		vsnprintf(buffer, size + 1, message, va);
		va_end(va);

		throw std::runtime_error(std::format("error -> {}", buffer));
		delete[] buffer;
	}
	#endif

	namespace tea {
		Configuration configuration = {
			.panic = panic
		};
	}
#endif
