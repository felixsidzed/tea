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

		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
			LLVMBuildBr(block, condBlock);
		LLVMPositionBuilderAtEnd(block, mergeBlock);
	}

	void CodeGen::emitForLoop(ForLoopNode* node) {
		LLVMBasicBlockRef condBlock = LLVMAppendBasicBlock(func, "loop.cond");
		LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlock(func, "loop.body");
		LLVMBasicBlockRef stepBlock = LLVMAppendBasicBlock(func, "loop.step");
		LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlock(func, "merge");

		LLVMBasicBlockRef oldBreakTarget = breakTarget;
		LLVMBasicBlockRef oldContinueTarget = continueTarget;
		breakTarget = mergeBlock;
		continueTarget = stepBlock;

		for (const auto& var : node->vars) {
			LLVMValueRef* prealloc = fnPrealloc->find(var);
			if (!prealloc)
				emitVariable(var);
			else
				locals.push({
					.type = var->dataType,
					.name = var->name,
					.allocated = *prealloc,
				});
		}

		LLVMBuildBr(block, condBlock);
		LLVMPositionBuilderAtEnd(block, stepBlock);
		emitAssignment(node->step.get());
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
		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
			LLVMBuildBr(block, stepBlock);

		breakTarget = oldBreakTarget;
		continueTarget = oldContinueTarget;

		LLVMPositionBuilderAtEnd(block, mergeBlock);
	}
}
