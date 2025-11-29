#include "codegen/codegen.h"

#include <fstream>
#include <filesystem>

#include "common/tea.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"

namespace fs = std::filesystem;

namespace tea {

	void CodeGen::emitVariable(const AST::VariableNode* node) {
		mir::Value* allocated = builder.alloca_(node->type, node->name + ".addr");
		if (node->initializer) {
			mir::Value* init = emitExpression(node->initializer.get());
			builder.store(allocated, init);
		}

		locals[std::hash<tea::string>()(node->name)] = {
			.allocated = allocated,
			.load = nullptr
		};
	}

} // namespace tea
