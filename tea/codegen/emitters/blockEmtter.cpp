#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitBlock(const Tree& root, const char* name, LLVMValueRef parent, std::pair<LLVMTypeRef, LLVMValueRef>* returnInto) {
		std::vector<Local> oldLocals = locals;

		if (parent) {
			LLVMBasicBlockRef _ = LLVMAppendBasicBlock(parent, name);
			block = LLVMCreateBuilder();
			LLVMPositionBuilderAtEnd(block, _); 
		}

		for (const auto& node : root) {
			switch (node->type) {
			case tnode(ReturnNode): {
				{
					int nattrs = LLVMGetAttributeCountAtIndex(func, LLVMAttributeFunctionIndex);
					LLVMAttributeRef* attrs = new LLVMAttributeRef[nattrs + 1];
					LLVMGetAttributesAtIndex(func, LLVMAttributeFunctionIndex, attrs);
					for (int i = 0; i < nattrs; i++) {
						if (LLVMGetEnumAttributeKind(attrs[i]) == LLVMAttrNoReturn) {
							delete[] attrs;
							if (((ReturnNode*)node.get())->value)
								TEA_PANIC("@noreturn function '%s' does return. line %d, column %d", LLVMGetValueName(func), node->line, node->column);
							else
								LLVMBuildUnreachable(block);
							return;
						}
					}
					delete[] attrs;
				}

				LLVMTypeRef expected = LLVMGetReturnType(LLVMGlobalGetValueType(func));
				if (LLVMGetTypeKind(expected) == LLVMVoidTypeKind) {
					if (((ReturnNode*)node.get())->value)
						TEA_PANIC("void function '%s' should not return a value. line %d, column %d", LLVMGetValueName(func), node->line, node->column);
					else {
						LLVMBuildRetVoid(block);
						return;
					}
				} else if (!((ReturnNode*)node.get())->value) {
					TEA_PANIC("non-void function '%s' should return a value. line %d, column %d", LLVMGetValueName(func), node->line, node->column);
					return;
				}

				auto [type, value] = emitExpression(((ReturnNode*)node.get())->value);
				if (!returnInto) {
					if (type != expected)
						TEA_PANIC("return value type (%s) is incompatible with function return type (%s). line %d, column %d",
							llvm2readable(type.llvm), llvm2readable(expected), node->line, node->column);
				} else {
					if (type != returnInto->first)
						TEA_PANIC("return value type (%s) is incompatible with function return type (%s). line %d, column %d",
							llvm2readable(type.llvm), llvm2readable(returnInto->first), node->line, node->column);
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
					LLVMTypeKind kind = LLVMGetTypeKind(type.llvm);
					switch (kind) {
					case LLVMIntegerTypeKind:
						pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstInt(type.llvm, 0, 0), "");
						break;
					case LLVMFloatTypeKind:
					case LLVMDoubleTypeKind:
						pred = LLVMBuildFCmp(block, LLVMRealONE, pred, LLVMConstReal(type.llvm, 0.0), "");
						break;
					default:
						pred = LLVMBuildICmp(block, LLVMIntNE, pred, LLVMConstNull(type.llvm), "");
						break;
					}
				}

				LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlock(func, "then");
				LLVMBasicBlockRef elseIfBlock = nullptr;
				if (ifNode->elseIf)
					elseIfBlock = LLVMAppendBasicBlock(func, "elseif");

				// keep block order
				int elseIfCount = 0;
				LLVMBasicBlockRef* elseIfThenBlocks = nullptr;
				if (ifNode->elseIf) {
					elseIfCount = 1;
					ElseIfNode* elseIf = ifNode->elseIf.get();
					while ((elseIf = elseIf->next.get()))
						elseIfCount++;
					elseIfThenBlocks = new LLVMBasicBlockRef[elseIfCount];
					for (int i = 0; i < elseIfCount; i++) {
						elseIfThenBlocks[i] = LLVMAppendBasicBlock(func, "then");
					}
				}

				LLVMBasicBlockRef elseBlock = ifNode->else_ ? LLVMAppendBasicBlock(func, "else") : nullptr;
				LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlock(func, "merge");

				LLVMBuildCondBr(block, pred, thenBlock, ifNode->elseIf ? elseIfBlock : elseBlock);

				LLVMPositionBuilderAtEnd(block, thenBlock);
				emitBlock(ifNode->body, "if.then", nullptr, returnInto);
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
					LLVMBuildBr(block, mergeBlock);

				ElseIfNode* elseIf = ifNode->elseIf.get();
				LLVMBasicBlockRef curElseIfBlock = elseIfBlock;
				for (int i = 0; i < elseIfCount; i++) {
					LLVMBasicBlockRef nextBlock = elseIf->next
						? LLVMAppendBasicBlock(func, "elseif")
						: (ifNode->else_ ? elseBlock : mergeBlock);

					LLVMPositionBuilderAtEnd(block, curElseIfBlock);

					auto [elseIfType, elseIfPred] = emitExpression(elseIf->pred);
					if (elseIfType != LLVMInt1Type()) {
						LLVMTypeKind kind = LLVMGetTypeKind(elseIfType.llvm);
						switch (kind) {
						case LLVMIntegerTypeKind:
							elseIfPred = LLVMBuildICmp(block, LLVMIntNE, elseIfPred, LLVMConstInt(elseIfType.llvm, 0, 0), "");
							break;
						case LLVMFloatTypeKind:
						case LLVMDoubleTypeKind:
							elseIfPred = LLVMBuildFCmp(block, LLVMRealONE, elseIfPred, LLVMConstReal(elseIfType.llvm, 0.0), "");
							break;
						default:
							elseIfPred = LLVMBuildICmp(block, LLVMIntNE, elseIfPred, LLVMConstNull(elseIfType.llvm), "");
							break;
						}
					}

					LLVMBasicBlockRef elseIfThen = elseIfThenBlocks[i];
					LLVMBuildCondBr(block, elseIfPred, elseIfThen, nextBlock);

					LLVMPositionBuilderAtEnd(block, elseIfThen);
					emitBlock(elseIf->body, "eleif.then", nullptr, returnInto);
					if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
						LLVMBuildBr(block, mergeBlock);

					curElseIfBlock = nextBlock;
					elseIf = elseIf->next.get();
				}

				if (ifNode->else_) {
					LLVMPositionBuilderAtEnd(block, elseBlock);
					emitBlock(ifNode->else_->body, "else", nullptr, returnInto);
					if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
						LLVMBuildBr(block, mergeBlock);
				}

				delete[] elseIfThenBlocks;

				LLVMPositionBuilderAtEnd(block, mergeBlock);
				break;
			}

			case tnode(AssignmentNode):
				emitAssignment((AssignmentNode*)node.get());
				break;

			case tnode(WhileLoopNode):
				emitWhileLoop((WhileLoopNode*)node.get());
				break;

			default:
				TEA_PANIC("invalid statement. line %d, column %d", node->line, node->column);
			}
		}

		//LLVMDisposeBuilder(block);
		locals = oldLocals;
	}
}
