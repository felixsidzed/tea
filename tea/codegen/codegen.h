#pragma once

#include <format>
#include <iostream>

#include "tea.h"
#include "parser/node.h"

#include <llvm-c/Core.h>

#define tnode(node) NODE_##node

namespace tea {
	extern LLVMTypeRef type2llvm[TYPE__COUNT];

	class CodeGen {
	public:
		bool is64Bit;
		bool verbose;

		CodeGen(bool is64bit = true, bool verbose = false) : is64Bit(is64bit), verbose(verbose) {}

		void emit(const Tree& tree, const char* output);

	private:
		LLVMModuleRef module = nullptr;
		LLVMValueRef func = nullptr;
		LLVMBuilderRef block = nullptr;

		std::unique_ptr<char, decltype(&LLVMDisposeMessage)> lastError = { nullptr, LLVMDisposeMessage };

		std::vector<std::pair<enum Type, std::string>>* curArgs = nullptr;

		inline void logUnformatted(const std::string& message) {
			if (!verbose)
				return;
			std::cout << " - " << message << std::endl;
		}

		template<typename... Args>
		inline void log(const std::string& fmt, Args ...args) {
			if (!verbose)
				return;
			logUnformatted(std::vformat(fmt, std::make_format_args(args...)));
		}

		void emitCode(const Tree& tree);
		void emitFunction(FunctionNode* tree);
		void emitBlock(const Tree& block, const char* name, LLVMValueRef parent);
		std::pair<LLVMTypeRef, LLVMValueRef> emitExpression(const std::unique_ptr<ExpressionNode>& node);
	};
}
