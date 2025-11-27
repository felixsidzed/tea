#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	using namespace frontend;

	void CodeGen::emitBlock(const AST::Tree& tree) {
		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Return: {
				AST::ReturnNode* ret = (AST::ReturnNode*)node.get();
				
				builder.ret(emitExpression(ret->value.get()));
			} break;

			default:
				TEA_PANIC("unknown node kind %d", node->kind);
			}
		}
	}

} // namespace tea
