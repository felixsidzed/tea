#pragma once

#include "flags.h"
#include "mir/mir.h"
#include "frontend/parser/AST.h"

namespace tea {

	using namespace frontend;

	class CodeGen {
		struct Local {
			mir::Value* allocated;
			mir::Value* loaded;
		};

		tea::Context& ctx;

		mir::Builder builder;
		std::unique_ptr<mir::Module> module = nullptr;
		tea::vector<std::pair<Type*, tea::string>>* curParams = nullptr;

		tea::map<tea::string, Local> locals;
		tea::umap<const StructType*, const AST::ObjectNode*> structMap;

		mir::Value* self = nullptr;
		mir::BasicBlock* contTarget = nullptr;
		mir::BasicBlock* breakTarget = nullptr;

		uint32_t fsrc;

		const char* curModuleName = nullptr;

	public:
		struct Options {
			tea::string triple;
			mir::DataLayout dl;
		};

		CodeGen(tea::Context& ctx) : ctx(ctx), builder(ctx) {}

		std::unique_ptr<mir::Module> emit(uint32_t fsrc, const AST::Tree& tree, const Options& options = {});

	private:
		void emitBlock(const AST::Tree* tree);
		void emitObject(const AST::ObjectNode* node);
		void emitVariable(const AST::VariableNode* node);
		mir::Function* emitFunctionImport(const AST::FunctionImportNode* node);
		mir::Value* emitExpression(const AST::ExpressionNode* expr, EmissionFlags flags = EmissionFlags::None, bool* asRef = nullptr);

		mir::Value* expr2bool(mir::Value* pred);
	};

} // namespace tea
