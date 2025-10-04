#include "../codegen.h"

#include "tea.h"
#include "lexer/token.h"

namespace tea {
	void CodeGen::emitAssignment(AssignmentNode* node) {
		bool ptr = false;
		auto [lhsType, lhsValue] = emitExpression(node->left, false, &ptr);
		if (!ptr)
			TEA_PANIC("lhs of assignment is not assignable. line %d, column %d", node->line, node->column);

		auto [rhsType, rhsValue] = emitExpression(node->right);

		if (rhsType.llvm != LLVMGetElementType(lhsType.llvm)) {
			TEA_PANIC("assignment type mismatch: cannot assign (%s) to (%s). line %d, column %d",
				llvm2readable(rhsType.llvm), llvm2readable(LLVMGetElementType(lhsType.llvm)), node->line, node->column);
		}

		LLVMValueRef finalValue = rhsValue;
		
		if (node->extra != 0) {
			LLVMValueRef current = LLVMBuildLoad2(block, lhsType.llvm, lhsValue, "");
			switch (node->extra) {
			case TOKEN_ADD: finalValue = LLVMBuildAdd(block, current, rhsValue, ""); break;
			case TOKEN_SUB: finalValue = LLVMBuildSub(block, current, rhsValue, ""); break;
			case TOKEN_MUL: finalValue = LLVMBuildMul(block, current, rhsValue, ""); break;
			case TOKEN_DIV: finalValue = LLVMBuildSDiv(block, current, rhsValue, ""); break;
			default:
				TEA_PANIC("unsupported extra assignment operator. line %d, column %d", node->line, node->column);
			}
		}
		
		LLVMBuildStore(block, finalValue, lhsValue);
	}
}
