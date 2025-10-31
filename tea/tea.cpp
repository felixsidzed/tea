#include "tea/tea.h"

#ifndef TEA_NODEFAULTCONFIG

#include <format>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define TEA_CURRENTTID GetCurrentThreadId()
#endif

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"

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
		.panic = panic,
		.is64Bit = true
	};
}

#endif

namespace tea {
	void compile(const std::string& src, const string& output, const vector<const char*>& importLookup, bool is64Bit, bool verbose, uint8_t optimization) {
		TEA_IS64BIT = is64Bit;
		Type::convert.clear();

		const auto& tokens = Lexer::tokenize(src);

		Parser parser;
		const auto& root = parser.parse(tokens);

		auto codegen = std::make_unique<CodeGen>(verbose, importLookup);
		codegen->emit(root, output, optimization);
	}
}

