#include "codegen/codegen.h"

namespace tea {

	void CodeGen::emitVariable(const AST::VariableNode* node) {
		mir::Value* allocated = builder.alloca_(node->type, node->name + ".addr");
		if (node->initializer)
			builder.store(allocated, emitExpression(node->initializer.get()));

		locals[node->name] = {
			.allocated = allocated,
			.loaded = nullptr
		};
	}

} // namespace tea
