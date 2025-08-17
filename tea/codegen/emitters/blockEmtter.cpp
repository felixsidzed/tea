#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitBlock(const Tree& root, const char* name, LLVMValueRef parent, std::pair<LLVMTypeRef, LLVMValueRef>* returnInto) {
		std::vector<Local> oldLocals = locals;

		if (name) {
			LLVMBasicBlockRef _ = LLVMAppendBasicBlock(parent, name);
			block = LLVMCreateBuilder();
			LLVMPositionBuilderAtEnd(block, _); 
		}

		for (const auto& node : root) {
			switch (node->type) {
			case tnode(ReturnNode): {
				LLVMTypeRef expected = LLVMGetReturnType(LLVMGlobalGetValueType(func));

				auto [type, value] = emitExpression(((ReturnNode*)node.get())->value);
				if (!returnInto) {
					if (type != expected)
						TEA_PANIC("return value (%s) is incompatible with function return type (%s). line %d, column %d",
							llvm2readable(type, value), llvm2readable(expected), node->line, node->column);
				} else {
					if (type != returnInto->first)
						TEA_PANIC("return value (%s) is incompatible with function return type (%s). line %d, column %d",
							llvm2readable(type, value), llvm2readable(returnInto->first), node->line, node->column);
				}

				if (returnInto)
					LLVMBuildStore(block, value, returnInto->second);
				else
					LLVMBuildRet(block, value);
				break;
			}

			case tnode(VariableNode):
				emitVariable((VariableNode*)node.get());
				break;

			case tnode(ExpressionNode):
				emitExpression((const std::unique_ptr<ExpressionNode>&)node);
				break;

			case tnode(IfNode): {
				IfNode* ifNode = (IfNode*)node.get();
				auto [type, pred] = emitExpression(ifNode->pred);

				if (type != LLVMInt1Type()) {
					LLVMTypeKind kind = LLVMGetTypeKind(type);
					switch (kind) {
					case LLVMIntegerTypeKind:
						pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstInt(type, 0, 0), "");
						break;
					case LLVMFloatTypeKind:
					case LLVMDoubleTypeKind:
						pred = LLVMBuildFCmp(block, LLVMRealONE, pred, LLVMConstReal(type, 0.0), "");
						break;
					default:
						pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstNull(type), "");
						break;
					}
				}

				LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlock(func, "then");
				LLVMBasicBlockRef elseBlock = nullptr;
				if (ifNode->else_)
					elseBlock = LLVMAppendBasicBlock(func, "else");
				LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlock(func, "merge");
				
				if (ifNode->else_) {
					LLVMBuildCondBr(block, pred, thenBlock, elseBlock);
				} else
					LLVMBuildCondBr(block, pred, thenBlock, mergeBlock);

				LLVMPositionBuilderAtEnd(block, thenBlock);
				emitBlock(ifNode->body, nullptr, func, returnInto);
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
					LLVMBuildBr(block, mergeBlock);

				if (ifNode->else_) {
					LLVMPositionBuilderAtEnd(block, elseBlock);
					emitBlock(ifNode->else_->body, nullptr, func, returnInto);
					if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
						LLVMBuildBr(block, mergeBlock);
				}

				LLVMPositionBuilderAtEnd(block, mergeBlock);
				break;
			}

			default:
				TEA_PANIC("invalid statement. line %d, column %d", node->line, node->column);
			}
		}

		LLVMDisposeBuilder(block);
		locals = oldLocals;
	}
}
