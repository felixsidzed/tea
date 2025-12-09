#include "codegen/codegen.h"

namespace tea {

	void CodeGen::emitVariable(const AST::VariableNode* node) {
		mir::Value* allocated = builder.alloca_(node->type, node->name + ".addr");
		if (node->initializer) {
			mir::Value* init = emitExpression(node->initializer.get());
			builder.store(allocated, init);
		}

		locals[std::hash<tea::string>()(node->name)] = {
			.allocated = allocated,
			.loaded = nullptr
		};
	}

} // namespace tea
