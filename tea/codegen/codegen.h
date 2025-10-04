#pragma once

#include <format>
#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "tea.h"
#include "parser/node.h"

#include <llvm-c/Core.h>

namespace tea {
	namespace fs = std::filesystem;

	extern LLVMTypeRef type2llvm[TYPE__COUNT];
	extern LLVMAttributeKind attr2llvm[ATTR__COUNT];

	class CodeGen {
	public:
		typedef std::unordered_map<std::string, LLVMValueRef> ImportedModule;
		
		bool is64Bit;
		bool verbose;
		fs::path importLookup;

		CodeGen(bool is64bit = true, bool verbose = false, fs::path importLookup = ".") : is64Bit(is64bit), verbose(verbose), importLookup(importLookup) {}

		void emit(const Tree& tree, const char* output);

	private:
		struct Local {
			enum Type type;
			std::string name;
			LLVMValueRef allocated;
		};

		LLVMModuleRef module = nullptr;
		LLVMValueRef func = nullptr;
		LLVMBuilderRef block = nullptr;

		std::unique_ptr<char, decltype(&LLVMDisposeMessage)> lastError = { nullptr, LLVMDisposeMessage };

		std::vector<struct Local> locals;
		std::unordered_map<std::string, ImportedModule> modules;
		std::vector<std::pair<enum Type, std::string>>* curArgs = nullptr;

		std::unordered_map<std::string, FunctionNode*> inlineables;
		std::vector<LLVMValueRef>* argsMap;

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
		void emitVariable(VariableNode* node);
		void emitFunctionImport(FunctionImportNode* node);
		void emitBlock(const Tree& block, const char* name, LLVMValueRef parent, std::pair<LLVMTypeRef, LLVMValueRef>* returnInto = nullptr);
		std::pair<LLVMTypeRef, LLVMValueRef> emitExpression(const std::unique_ptr<ExpressionNode>& node, bool constant = false);

		static const char* llvm2readable(LLVMTypeRef type);
	};
}
