#include "../codegen.h"
#include "tea.h"

namespace tea {
	std::pair<LLVMTypeRef, LLVMValueRef> CodeGen::emitExpression(const std::unique_ptr<ExpressionNode>& node) {
		switch (node->etype) {
		case EXPR_INT: {
			return {
				type2llvm[TYPE_INT],
				LLVMConstInt(type2llvm[TYPE_INT], std::stoll(node->value), true)
			};
		} break;

		case EXPR_FLOAT: {
			return {
				type2llvm[TYPE_FLOAT],
				LLVMConstReal(type2llvm[TYPE_FLOAT], std::stod(node->value))
			};
		} break;

		case EXPR_STRING: {
			return {
				type2llvm[TYPE_STRING],
				LLVMBuildGlobalStringPtr(block, node->value.c_str(), "$string")
			};
		} break;

		default:
			TEA_PANIC("invalid expression. line %d, column %d", node->line, node->column);
			TEA_UNREACHABLE();
		}
	}
}
