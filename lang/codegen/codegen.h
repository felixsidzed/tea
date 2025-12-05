#pragma once

#include "flags.h"
#include "mir/mir.h"
#include "frontend/parser/AST.h"

namespace tea {

	using namespace frontend;

	class CodeGen {
		struct Local {
			mir::Value* allocated;
			mir::Value* load;
		};
		typedef tea::map<tea::string, mir::Function*> ImportedModule;

		mir::Builder builder;
		std::unique_ptr<mir::Module> module = nullptr;
		tea::vector<std::pair<Type*, tea::string>>* curParams = nullptr;

		tea::map<size_t, Local> locals;
		tea::map<size_t, ImportedModule> importedModules;

		mir::BasicBlock* contTarget = nullptr;
		mir::BasicBlock* breakTarget = nullptr;

	public:
		struct Options {
			tea::string triple;
			mir::DataLayout dl;
		};

		tea::vector<const char*> importLookup;

		std::unique_ptr<mir::Module> emit(const AST::Tree& tree, const Options& options = {});

	private:
		void emitBlock(const AST::Tree* tree);
		void emitVariable(const AST::VariableNode* node);
		void emitModuleImport(const AST::ModuleImportNode* node);
		mir::Function* emitFunctionImport(const AST::FunctionImportNode* node);
		mir::Value* emitExpression(const AST::ExpressionNode* expr, EmissionFlags flags = EmissionFlags::None, bool* asRef = nullptr);

		mir::Value* expr2bool(mir::Value* pred);
	};

} // namespace tea
