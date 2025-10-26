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

	// TODO: reordering verbose/importLookup/objects seems to break field construction, for some reason??
	class CodeGen {
	public:
		fs::path importLookup{};
		bool verbose = false;

		CodeGen(bool verbose = false, const fs::path& importLookup = ".") : verbose(verbose), importLookup(importLookup) {}

		void emit(const Tree& tree, const char* output);

	private:
		typedef map<string, LLVMValueRef> ImportedModule;

		struct Local {
			Type type;
			string name;
			LLVMValueRef allocated;
		};

		bool fnDeduceRetTy = false;
		bool hasNoNamespaceFunctions = false;

		LLVMValueRef self = nullptr;
		LLVMValueRef func = nullptr;
		LLVMModuleRef module = nullptr;
		LLVMBuilderRef block = nullptr;
		FunctionNode* curFunc = nullptr;
		LLVMBasicBlockRef breakTarget = nullptr;
		vector<LLVMValueRef>* argsMap = nullptr;
		LLVMBasicBlockRef continueTarget = nullptr;
		vector<std::pair<Type, string>>* curArgs = nullptr;
		map<VariableNode*, LLVMValueRef>* fnPrealloc = nullptr;

		vector<struct Local> locals{};
		map<string, ImportedModule> modules{};
		map<LLVMTypeRef, ObjectNode*> objects{};
		map<string, FunctionNode*> inlineables{};

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
		void emitObject(ObjectNode* node);
		void emitForLoop(ForLoopNode* node);
		void emitFunction(FunctionNode* tree);
		void emitVariable(VariableNode* node);
		void emitWhileLoop(WhileLoopNode* node);
		void emitAssignment(AssignmentNode* node);
		LLVMValueRef emitFunctionImport(FunctionImportNode* node);
		void emitBlock(const Tree& block, const char* name, LLVMValueRef parent, std::pair<LLVMTypeRef, LLVMValueRef>* returnInto = nullptr);
		std::pair<Type, LLVMValueRef> emitExpression(const std::unique_ptr<ExpressionNode>& node, bool constant = false, bool* ptr = nullptr);

		static string type2readable(const Type& type);
	};
}
