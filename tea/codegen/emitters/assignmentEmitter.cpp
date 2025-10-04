#include "../codegen.h"

#include "tea/tea.h"
#include "tea/lexer/token.h"

namespace tea {
	void CodeGen::emitAssignment(AssignmentNode* node) {
		bool ptr = false;
		auto [lhsType, lhsValue] = emitExpression(node->left, false, &ptr);

		auto [rhsType, rhsValue] = emitExpression(node->right);

		if (rhsType.llvm != LLVMGetElementType(lhsType.llvm)) {
			TEA_PANIC("assignment type mismatch: cannot assign %s to %s. line %d, column %d",
				type2readable(rhsType), type2readable(LLVMGetElementType(lhsType.llvm)), node->line, node->column);
		}

		LLVMValueRef finalValue = rhsValue;
		
		if (node->extra != 0) {
			LLVMValueRef current = LLVMBuildLoad(block, lhsValue, "");
			switch (node->extra) {
			case TOKEN_ADD: finalValue = LLVMBuildAdd(block, current, rhsValue, ""); break;
			case TOKEN_SUB: finalValue = LLVMBuildSub(block, current, rhsValue, ""); break;
			case TOKEN_MUL: finalValue = LLVMBuildMul(block, current, rhsValue, ""); break;
			case TOKEN_DIV: finalValue = LLVMBuildSDiv(block, current, rhsValue, ""); break;
			default:
				TEA_PANIC("unsupported extra operator in assignment. line %d, column %d", node->line, node->column);
			}
		}
		
		LLVMBuildStore(block, finalValue, lhsValue);
	}
}
