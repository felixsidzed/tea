#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	mir::Value* expr2bool(mir::Builder* builder, mir::Module* module, mir::Value* pred) {
		if (pred->type->kind != TypeKind::Bool) {
			switch (pred->type->kind) {
			case TypeKind::Int:
				pred = builder->icmp(
					mir::ICmpPredicate::NEQ, pred,
					mir::ConstantNumber::get(0, module->getSize(pred->type) * 8, pred->type->sign), ""
				);
				break;

			case TypeKind::Float:
			case TypeKind::Double:
				pred = builder->fcmp(
					mir::FCmpPredicate::ONEQ, pred,
					mir::ConstantNumber::get<double>(0.0, module->getSize(pred->type) * 8, pred->type->sign), ""
				);
				break;

			default:
				break;
			}
		}
		return pred;
	}

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
				
				mir::Value* pred = expr2bool(&builder, module.get(), emitExpression(ifNode->pred.get()));

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

					mir::Value* elifPred = expr2bool(&builder, module.get(), emitExpression(elseifNode->pred.get()));
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

				//builder.insertInto(merge);
			} break;

			case AST::NodeKind::WhileLoop: {
				AST::WhileLoopNode* loop = (AST::WhileLoopNode*)node.get();
				mir::Function* func = builder.getInsertBlock()->parent;

				mir::BasicBlock* pred = func->appendBlock("loop.pred");
				mir::BasicBlock* body = func->appendBlock("loop.body");
				mir::BasicBlock* merge = func->appendBlock("loop.merge");

				builder.br(pred);
				builder.cbr(
					expr2bool(&builder, module.get(), emitExpression(loop->pred.get())),
					body, merge
				);

				builder.insertInto(body);
				emitBlock(&loop->body);

				if (!builder.getInsertBlock()->getTerminator())
					builder.br(pred);
				builder.insertInto(merge);
			} break;

			case AST::NodeKind::Assignment:
				emitAssignment((const AST::AssignmentNode*)node.get());
				break;

			default:
				TEA_PANIC("unknown statement %d", node->kind);
			}
		}

		locals = oldLocals;
	}

} // namespace tea
