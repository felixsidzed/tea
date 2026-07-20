#include "codegen/codegen.h"

#include <fstream>
#include <filesystem>

#include "core/tea.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"

namespace fs = std::filesystem;

namespace tea {

	mir::Function* CodeGen::emitFunctionImport(const AST::FunctionImportNode* node) {
		tea::vector<Type*> params;
		for (const auto& [ty, _] : node->params)
			params.emplace(ty);

		mir::Function* func = module->addFunction(node->name, ctx.types.Function(node->returnType, params, node->vararg));
		func->storage = AST::StorageClass::Public;
		func->subclassData = node->extra;
		return func;
	}

} // namespace tea
