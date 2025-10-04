#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	void CodeGen::emitWhileLoop(WhileLoopNode* node) {
		LLVMBasicBlockRef condBlock = LLVMAppendBasicBlock(func, "loop.cond");
		LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlock(func, "loop.body");
		LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlock(func, "merge");

		LLVMBuildBr(block, condBlock);
		LLVMPositionBuilderAtEnd(block, condBlock);

		auto [predType, pred] = emitExpression(node->pred);
		if (predType != LLVMInt1Type()) {
			LLVMTypeKind kind = LLVMGetTypeKind(predType.llvm);
			switch (kind) {
			case LLVMIntegerTypeKind:
				pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstInt(predType.llvm, false, false), "");
				break;
			case LLVMFloatTypeKind:
			case LLVMDoubleTypeKind:
				pred = LLVMBuildFCmp(block, LLVMRealONE, pred, LLVMConstReal(predType.llvm, 0.0), "");
				break;
			default:
				pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstNull(predType.llvm), "");
				break;
			}
		}

		LLVMBuildCondBr(block, pred, bodyBlock, mergeBlock);
		LLVMPositionBuilderAtEnd(block, bodyBlock);

		emitBlock(node->body, "loop.body", nullptr);

		if (!LLVMGetBasicBlockTerminator(bodyBlock))
			LLVMBuildBr(block, condBlock);
		LLVMPositionBuilderAtEnd(block, mergeBlock);
	}
}
