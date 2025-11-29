#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	void CodeGen::emitBlock(const AST::Tree& tree) {
		tea::map<size_t, Local> oldLocals = locals;

		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Return:
				builder.ret(emitExpression(((AST::ReturnNode*)node.get())->value.get()));
				break;

			case AST::NodeKind::Expression:
				emitExpression((const AST::ExpressionNode*)node.get());
				break;

			case AST::NodeKind::Variable:
				emitVariable((const AST::VariableNode*)node.get());
				break;

			default:
				TEA_PANIC("unknown statement %d", node->kind);
			}
		}

		locals = oldLocals;
	}

} // namespace tea
