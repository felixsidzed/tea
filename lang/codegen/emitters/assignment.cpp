#include "codegen/codegen.h"

#include "common/tea.h"
#include "frontend/lexer/Lexer.h"

namespace tea {

	void CodeGen::emitAssignment(const AST::AssignmentNode* node) {
		bool isRef = false;
		mir::Value* lhs = emitExpression(node->lhs.get(), EmissionFlags::None, &isRef);
		mir::Value* rhs = emitExpression(node->rhs.get());

		if (!isRef || lhs->type->constant)
			TEA_PANIC("cannot assign to a value of type '%s'. line %d, column %d",
				lhs->type->str().data(), node->line, node->column);

		if (node->extra != 0) {
			mir::Value* cur = builder.load(lhs, "");
			switch ((TokenKind)node->extra) {
			case TokenKind::Add: rhs = builder.arithm(mir::OpCode::Add, cur, rhs, ""); break;
			case TokenKind::Sub: rhs = builder.arithm(mir::OpCode::Sub, cur, rhs, ""); break;
			case TokenKind::Div: rhs = builder.arithm(mir::OpCode::Div, cur, rhs, ""); break;
			case TokenKind::Star: rhs = builder.arithm(mir::OpCode::Mul, cur, rhs, ""); break;
			default:
				TEA_PANIC("invalid extra operator in assignment. line %d, column %d", node->line, node->column);
			}
		}

		builder.store(lhs, rhs);
	}

} // namespace tea
