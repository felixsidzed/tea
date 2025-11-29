#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	void CodeGen::emitBlock(const AST::Tree& tree) {
		tea::map<size_t, Local> oldLocals = locals;

		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Return:
				builder.ret(emitExpression(((AST::ReturnNode*)node.get())->value.get()));
				break;

			case AST::NodeKind::Expression:
				emitExpression((const AST::ExpressionNode*)node.get());
				break;

			case AST::NodeKind::Variable:
				emitVariable((const AST::VariableNode*)node.get());
				break;

			case AST::NodeKind::If: {
				AST::IfNode* ifNode = (AST::IfNode*)node.get();

				mir::Value* pred = emitExpression(ifNode->pred.get());
				if (pred->type->kind != TypeKind::Bool) {
					switch (pred->type->kind) {
					case TypeKind::Int:
						pred = builder.icmp(
							mir::ICmpPredicate::NEQ, pred,
							mir::ConstantNumber::get(0, module->getSize(pred->type) * 8, pred->type->sign),
							""
						);
						break;
					case TypeKind::Float:
					case TypeKind::Double:
						pred = builder.fcmp(
							mir::FCmpPredicate::ONEQ, pred,
							mir::ConstantNumber::get<double>(0.0, module->getSize(pred->type) * 8, pred->type->sign),
							""
						);
						break;
					default:
						break;
					}
				}

				mir::Function* func = builder.getInsertBlock()->parent;
				mir::BasicBlock* then = func->appendBlock("if.then");
				mir::BasicBlock* otherwise = nullptr;
				if (ifNode->otherwise)
					otherwise = func->appendBlock("if.else");
				mir::BasicBlock* merge = func->appendBlock("if.merge");

				builder.cbr(pred, then, otherwise ? otherwise : merge);

				builder.insertInto(then);
				emitBlock(ifNode->body);
				if (!builder.getInsertBlock()->getTerminator())
					builder.br(merge);

				if (otherwise) {
					builder.insertInto(otherwise);
					emitBlock(ifNode->otherwise->body);
					if (!builder.getInsertBlock()->getTerminator())
						builder.br(merge);
				}
			} break;

			default:
				TEA_PANIC("unknown statement %d", node->kind);
			}
		}

		locals = oldLocals;
	}

} // namespace tea
