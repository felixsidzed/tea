#pragma once

#include <format>
#include <iostream>
#include <filesystem>
#include <unordered_map>

#include "tea/tea.h"
#include "tea/vector.h"
#include "tea/parser/node.h"

#include <llvm-c/Core.h>

namespace tea {
	namespace fs = std::filesystem;

	class CodeGen {
	public:
		typedef map<string, LLVMValueRef> ImportedModule;
		
		bool verbose;
		fs::path importLookup;

		CodeGen(bool verbose = false, fs::path importLookup = ".") : verbose(verbose), importLookup(importLookup) {}

		void emit(const Tree& tree, const char* output);

	private:
		struct Local {
			Type type;
			string name;
			LLVMValueRef allocated;
		};

		LLVMModuleRef module = nullptr;
		LLVMValueRef func = nullptr;
		LLVMBuilderRef block = nullptr;

		vector<struct Local> locals;
		map<string, ImportedModule> modules;
		vector<std::pair<Type, string>>* curArgs = nullptr;

		vector<LLVMValueRef>* argsMap = nullptr;
		map<string, FunctionNode*> inlineables;

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
		void emitWhileLoop(WhileLoopNode* node);
		void emitAssignment(AssignmentNode* node);
		LLVMValueRef emitFunctionImport(FunctionImportNode* node);
		void emitBlock(const Tree& block, const char* name, LLVMValueRef parent, std::pair<LLVMTypeRef, LLVMValueRef>* returnInto = nullptr);
		std::pair<Type, LLVMValueRef> emitExpression(const std::unique_ptr<ExpressionNode>& node, bool constant = false, bool* ptr = nullptr);

		static const char* llvm2readable(LLVMTypeRef type);
	};
}
