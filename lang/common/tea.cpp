#include "tea.h"

#include <fstream>

#include "codegen/codegen.h"
#include "backend/lowering.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "frontend/semantics/SemanticAnalyzer.h"

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

namespace tea {
	void compile(const tea::string& source, const tea::vector<const char*>& importLookup, const char* outFile, const char* triple, bool verbose, uint8_t optLevel) {
		clock_t start = clock();

		const tea::vector<tea::frontend::Token>& tokens = tea::frontend::lex(source);

		tea::frontend::Parser parser(tokens);
		const tea::frontend::AST::Tree& ast = parser.parse();

		tea::frontend::analysis::SemanticAnalyzer analyzer;
		analyzer.importLookup = importLookup;

		const auto& errors = analyzer.visit(ast);
		if (!errors.empty()) {
			fprintf(stderr, "%d error%c:\n", errors.size, errors.size == 1 ? 0 : 's');
			for (const auto& error : errors)
				fprintf(stderr, "  %.*s\n", (int)error.length(), error.data());
			return;
		}

		tea::CodeGen codegen;
		codegen.importLookup = importLookup;

		tea::CodeGen::Options coptions;
		if (triple)
			coptions.triple = triple;
		auto module = codegen.emit(ast, coptions);

		tea::backend::MIRLowering lowering;
		lowering.lower(module.get(), {
			.OutputFile = outFile,
			.DumpLLVMModule = verbose,
			.OptimizationLevel = optLevel,
		});

		double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		printf("Compilation took %ldm %lds %ldms\n",
			(long)(diff / 60),
			(long)diff % 60,
			(long)((diff - (long)diff) * 1000));
	}
}
