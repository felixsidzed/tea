#pragma once

#include <memory>

#include "common/Type.h"
#include "common/string.h"
#include "common/vector.h"

namespace tea::frontend::AST {
	enum class NodeKind {
		Function,
		Return,
		Expression
	};

	enum class ExprKind {
		String, Char, Int, Float, Double,
		Identf, /*Call,
		Add, Sub, Mul, Div,
		Eq, Neq, Lt, Gt, Le, Ge,
		Not, And, Or,
		Ref, Deref, Cast,
		Array, Index,
		Band, Bor, Bxor, Shr, Shl*/
	};

	enum class StorageClass {
		Public, Private
	};

	struct Node {
		NodeKind kind;
		uint32_t line, column;

		explicit Node(NodeKind kind, uint32_t line, uint32_t column)
			: kind(kind), line(line), column(column) {
		}
		virtual ~Node() = default;
	};

	typedef tea::vector<std::unique_ptr<Node>> Tree;

	struct ExpressionNode : Node {
		ExprKind ekind;
		Type* type;

		ExpressionNode(ExprKind ekind, uint32_t line, uint32_t column)
			: Node(NodeKind::Expression, line, column), ekind(ekind), type(nullptr) {
		}
	};

	struct LiteralNode : ExpressionNode {
		tea::string value;

		LiteralNode(ExprKind ekind, const tea::string& val, uint32_t line, uint32_t column)
			: ExpressionNode(ekind, line, column), value(val) {
		}
	};

	/*struct BinaryExpr : ExpressionNode {
		std::unique_ptr<ExpressionNode> lhs;
		std::unique_ptr<ExpressionNode> rhs;

		BinaryExpr(ExprKind ekind, std::unique_ptr<ExpressionNode> lhs, std::unique_ptr<ExpressionNode> rhs,
				   uint32_t line, uint32_t column)
			: ExpressionNode(ekind, line, column), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
	};*/

	struct FunctionNode : Node {
		StorageClass vis;
		tea::string name;
		tea::vector<std::pair<Type*, tea::string>> params;
		Type* returnType;

		Tree body;

		FunctionNode(
			StorageClass vis,
			const tea::string& name,
			const tea::vector<std::pair<Type*, tea::string>>& params,
			Type* retType,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::Function, line, column),
			vis(vis), name(name), params(params), returnType(retType) {
		}
	};

	struct ReturnNode : Node {
		std::unique_ptr<ExpressionNode> value;

		ReturnNode(std::unique_ptr<ExpressionNode> value, uint32_t line, uint32_t column)
			: Node(NodeKind::Return, line, column), value(std::move(value)) {
		}
	};
}
