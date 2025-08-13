#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitBlock(const Tree& root, const char* name, LLVMValueRef parent) {
		std::vector<Local> oldLocals = locals;

		LLVMBasicBlockRef _ = LLVMAppendBasicBlock(parent, name);
		block = LLVMCreateBuilder();
		LLVMPositionBuilderAtEnd(block, _);

		for (const auto& node : root) {
			switch (node->type) {
			case tnode(ReturnNode): {
				LLVMTypeRef expected = LLVMGetReturnType(LLVMGlobalGetValueType(func));
				auto [type, value] = emitExpression(((ReturnNode*)node.get())->value);
				if (type != expected) {
					TEA_PANIC("return value (%s) is incompatible with function return type (%s). line %d, column %d",
						llvm2readable(type, value), llvm2readable(expected), node->line, node->column);
				}
				LLVMBuildRet(block, value);
				break;
			}

			case tnode(VariableNode):
				emitVariable((VariableNode*)node.get());
				break;

			case tnode(ExpressionNode):
				emitExpression((const std::unique_ptr<ExpressionNode>&)node);
				break;

			default:
				TEA_PANIC("invalid statement. line %d, column %d", node->line, node->column);
			}
		}

		LLVMDisposeBuilder(block);

		locals = oldLocals;
	}
}
