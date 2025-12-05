#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	void CodeGen::emitBlock(const AST::Tree* tree) {
		tea::map<size_t, Local> oldLocals = locals;

		for (const auto& node : *tree) {
			switch (node->kind) {
			case AST::NodeKind::Return:
				builder.ret(emitExpression(((AST::ReturnNode*)node.get())->value.get()));
				break;

			case AST::NodeKind::Expression:
				emitExpression((const AST::ExpressionNode*)node.get());
				break;

			case AST::NodeKind::If: {
				AST::IfNode* ifNode = (AST::IfNode*)node.get();

				mir::Function* func = builder.getInsertBlock()->parent;
				mir::BasicBlock* merge = func->appendBlock("if.merge");
				
				mir::Value* pred = expr2bool(emitExpression(ifNode->pred.get()));

				mir::BasicBlock* falseTarget = nullptr;
				mir::BasicBlock* thenBlock = func->appendBlock("if.then");

				AST::ElseIfNode* elseifNode = ifNode->elseIf.get();
				if (elseifNode)
					falseTarget = func->appendBlock("if.elseif.cond");
				else if (ifNode->otherwise)
					falseTarget = func->appendBlock("if.else");
				else
					falseTarget = merge;
				builder.cbr(pred, thenBlock, falseTarget);

				builder.insertInto(thenBlock);
				emitBlock(&ifNode->body);
				if (!builder.getInsertBlock()->getTerminator())
					builder.br(merge);

				mir::BasicBlock* curCondBlock = falseTarget;
				while (elseifNode) {
					builder.insertInto(curCondBlock);

					mir::Value* elifPred = expr2bool(emitExpression(elseifNode->pred.get()));
					mir::BasicBlock* elifThenBlock = func->appendBlock("if.elseif.then");

					if (elseifNode->next)
						curCondBlock = func->appendBlock("if.elseif.cond");
					else if (ifNode->otherwise)
						curCondBlock = func->appendBlock("if.else");
					else
						curCondBlock = merge;
					builder.cbr(elifPred, elifThenBlock, curCondBlock);

					builder.insertInto(elifThenBlock);
					emitBlock(&elseifNode->body);
					if (!builder.getInsertBlock()->getTerminator())
						builder.br(merge);

					elseifNode = elseifNode->next.get();
				}

				if (ifNode->otherwise) {
					builder.insertInto(curCondBlock);
					emitBlock(&ifNode->otherwise->body);
					if (!builder.getInsertBlock()->getTerminator())
						builder.br(merge);
				}

				builder.insertInto(merge);
			} break;

			case AST::NodeKind::WhileLoop: {
				AST::WhileLoopNode* loop = (AST::WhileLoopNode*)node.get();
				mir::Function* func = builder.getInsertBlock()->parent;

				mir::BasicBlock* pred = func->appendBlock("loop.pred");
				mir::BasicBlock* body = func->appendBlock("loop.body");
				mir::BasicBlock* merge = func->appendBlock("loop.merge");

				contTarget = pred;
				breakTarget = merge;

				builder.br(pred);
				builder.cbr(
					expr2bool(emitExpression(loop->pred.get())),
					body, merge
				);

				builder.insertInto(body);
				emitBlock(&loop->body);

				if (!builder.getInsertBlock()->getTerminator())
					builder.br(pred);
				builder.insertInto(merge);

				contTarget = nullptr;
				breakTarget = nullptr;
			} break;

			case AST::NodeKind::LoopInterrupt: {
				uint32_t extra = node->extra;

				switch (extra) {
				case 0:
					builder.br(breakTarget, false);
					break;
				case 1:
					builder.br(contTarget, false);
					break;
				default:
					TEA_UNREACHABLE();
				}
			} break;

			case AST::NodeKind::ForLoop: {
				AST::ForLoopNode* loop = (AST::ForLoopNode*)node.get();
				mir::Function* func = builder.getInsertBlock()->parent;

				mir::BasicBlock* pred = func->appendBlock("loop.pred");
				mir::BasicBlock* body = func->appendBlock("loop.body");
				mir::BasicBlock* merge = func->appendBlock("loop.merge");

				contTarget = pred;
				breakTarget = merge;

				emitVariable(loop->var.get());

				builder.br(pred);
				if (loop->pred)
					builder.cbr(
						expr2bool(emitExpression(loop->pred.get())),
						body, merge
					);
				else
					builder.br(body);

				builder.insertInto(body);
				emitBlock(&loop->body);

				mir::Instruction* term = (mir::Instruction*)builder.getInsertBlock()->getTerminator();
				if (!term) {
					emitExpression(loop->step.get());
					builder.br(pred);
				} else if (term->op == mir::OpCode::Br && ((mir::BasicBlock*)term->operands[0]) == contTarget) {
					term->op = mir::OpCode::Nop;
					term->operands.clear();

					emitExpression(loop->step.get());
					builder.br(pred);
				}
				builder.insertInto(merge);

				contTarget = nullptr;
				breakTarget = nullptr;
			} break;

			default:
				TEA_PANIC("unknown statement %d", node->kind);
			}
		}

		locals = oldLocals;
	}

} // namespace tea
