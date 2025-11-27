#pragma once

#include <memory>

#include "common/Type.h"
#include "common/string.h"
#include "common/vector.h"

namespace tea::frontend::AST {
	enum class NodeKind: uint32_t {
		Function,
		Return,
		Expression,
		Call
	};

	enum class ExprKind : uint32_t {
		String, Char, Int, Float, Double,
		Identf, Call,
		/*Add, Sub, Mul, Div,
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
		uint32_t extra;
		uint32_t line, column;

		explicit Node(NodeKind kind, uint32_t line, uint32_t column, uint32_t extra = 0)
			: kind(kind), line(line), column(column), extra(extra) {
		}
		virtual ~Node() = default;
	};

	typedef tea::vector<std::unique_ptr<Node>> Tree;

	struct ExpressionNode : Node {
		Type* type;

		ExpressionNode(ExprKind ekind, uint32_t line, uint32_t column)
			: Node(NodeKind::Expression, line, column, (uint32_t)ekind), type(nullptr) {
		}

		ExprKind getEKind() const { return (ExprKind)extra; }
		void setEKind(ExprKind ekind) { extra = (uint32_t)ekind; }
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
			name(name), params(params), returnType(retType) {
			setVisibility(vis);
		}

		bool isVarArg() const { return (extra & 1) != 0; }
		void setVarArg(bool vararg) { extra = (extra & ~1) | (vararg ? 1 : 0); }

		StorageClass getVisibility() const { return (StorageClass)((extra & 6) >> 1); }
		void setVisibility(StorageClass vis) { extra = (extra & ~6) | (((uint32_t)vis << 1) & 6); }
	};

	struct ReturnNode : Node {
		std::unique_ptr<ExpressionNode> value;

		ReturnNode(std::unique_ptr<ExpressionNode> value, uint32_t line, uint32_t column)
			: Node(NodeKind::Return, line, column), value(std::move(value)) {
		}
	};

	struct CallNode : ExpressionNode {
		std::unique_ptr<ExpressionNode> callee;
		tea::vector<std::unique_ptr<ExpressionNode>> args;

		CallNode(
			std::unique_ptr<ExpressionNode> callee,
			tea::vector<std::unique_ptr<ExpressionNode>>&& args,
			uint32_t _line, uint32_t _column
		) : args(std::move(args)), callee(std::move(callee)), ExpressionNode(ExprKind::Call, _line, _column) {}
	};
}
