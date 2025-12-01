#pragma once

#include <memory>

#include "common/Type.h"
#include "common/string.h"
#include "common/vector.h"

namespace tea::frontend::AST {
	enum class NodeKind: uint32_t {
		Function, Return,
		Expression,
		Call,
		FunctionImport, ModuleImport,
		Variable, GlobalVariable,
		If, Else, ElseIf
	};

	enum class ExprKind : uint32_t {
		String, Char, Int, Float, Double,
		Identf, Call,
		Add, Sub, Mul, Div,
		Eq, Neq, Lt, Gt, Le, Ge,
		//Not, And, Or,
		Ref, Deref, Cast,
		/*Array, Index,
		Band, Bor, Bxor, Shr, Shl*/
	};

	enum class StorageClass : uint8_t {
		Public, Private
	};

	enum class CallingConvention : uint8_t {
		Std, Fast, C, Auto
	};

	enum class FunctionAttribute {
		Inline = 1 << 0,
		NoReturn = 1 << 1,
		NoNamespace = 1 << 2
	};

	enum class GlobalAttribute {
		ThreadLocal = 1 << 0
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

	struct BinaryExpr : ExpressionNode {
		std::unique_ptr<ExpressionNode> lhs;
		std::unique_ptr<ExpressionNode> rhs;

		BinaryExpr(ExprKind ekind, std::unique_ptr<ExpressionNode> lhs, std::unique_ptr<ExpressionNode> rhs,
				   uint32_t line, uint32_t column)
			: ExpressionNode(ekind, line, column), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
	};

	struct FunctionNode : Node {
		CallingConvention cc;
		StorageClass vis;
		bool vararg;
		tea::string name;
		tea::vector<std::pair<Type*, tea::string>> params;
		Type* returnType;

		Tree body;

		FunctionNode(
			StorageClass vis,
			CallingConvention cc,
			const tea::string& name,
			const tea::vector<std::pair<Type*, tea::string>>& params,
			Type* retType, bool vararg,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::Function, line, column),
			name(name), params(params), returnType(retType), vis(vis), cc(cc), vararg(vararg) {
		}

		void clearAttributes() { extra = 0; };
		void addAttribute(FunctionAttribute attr) { extra |= (uint32_t)attr; };
		bool hasAttribute(FunctionAttribute attr) const { return extra & (uint32_t)attr; };
		void removeAttribute(FunctionAttribute attr) { extra &= ~(uint32_t)attr; };
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

	struct FunctionImportNode : Node {
		CallingConvention cc;
		bool vararg;
		tea::string name;
		tea::vector<std::pair<Type*, tea::string>> params;
		Type* returnType;

		FunctionImportNode(
			CallingConvention cc,
			const tea::string& name,
			const tea::vector<std::pair<Type*, tea::string>>& params,
			Type* retType, bool vararg,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::FunctionImport, line, column),
			name(name), params(params), returnType(retType), cc(cc), vararg(vararg) {
		}

		void clearAttributes() { extra = 0; };
		void addAttribute(FunctionAttribute attr) { extra |= (uint32_t)attr; };
		bool hasAttribute(FunctionAttribute attr) const { return extra & (uint32_t)attr; };
		void removeAttribute(FunctionAttribute attr) { extra &= ~(uint32_t)attr; };
	};

	struct ModuleImportNode : Node {
		tea::string path;
		
		ModuleImportNode(
			const tea::string& path,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::ModuleImport, line, column), path(path) {
		}
	};

	struct VariableNode : Node {
		tea::string name;
		Type* type;
		std::unique_ptr<ExpressionNode> initializer;

		VariableNode(
			const tea::string& name,
			Type* type,
			std::unique_ptr<ExpressionNode> initializer,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::Variable, line, column), name(name), type(type), initializer(std::move(initializer)) {
		}
	};

	struct ElseNode : Node {
		Tree body;

		ElseNode(uint32_t line, uint32_t column)
			: Node(NodeKind::Else, line, column) {
		}
	};

	struct ElseIfNode : Node {
		std::unique_ptr<ExpressionNode> pred;
		std::unique_ptr<struct ElseIfNode> next;

		Tree body;

		ElseIfNode(
			std::unique_ptr<ExpressionNode> pred,
			std::unique_ptr<struct ElseIfNode> next,
			uint32_t line, uint32_t column
		) : Node(NodeKind::ElseIf, line, column),
			pred(std::move(pred)), next(std::move(next)) {
		}
	};

	struct IfNode : Node {
		std::unique_ptr<ExpressionNode> pred;
		std::unique_ptr<ElseNode> otherwise;
		std::unique_ptr<ElseIfNode> elseIf;

		Tree body;

		IfNode(
			std::unique_ptr<ExpressionNode> pred,
			uint32_t line, uint32_t column
		) : Node(NodeKind::If, line, column), pred(std::move(pred)) {
		}
	};

	struct GlobalVariableNode : Node {
		tea::string name;
		Type* type;
		std::unique_ptr<ExpressionNode> initializer;
		StorageClass vis;

		GlobalVariableNode(
			const tea::string& name,
			Type* type,
			std::unique_ptr<ExpressionNode> initializer,
			StorageClass vis,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::GlobalVariable, line, column),
			name(name), type(type), initializer(std::move(initializer)), vis(vis) {
		}

		void clearAttributes() { extra = 0; };
		void addAttribute(GlobalAttribute attr) { extra |= (uint32_t)attr; };
		bool hasAttribute(GlobalAttribute attr) const { return extra & (uint32_t)attr; };
		void removeAttribute(GlobalAttribute attr) { extra &= ~(uint32_t)attr; };
	};

	struct ReferenceNode : ExpressionNode {
		std::unique_ptr<ExpressionNode> value;

		ReferenceNode(
			std::unique_ptr<ExpressionNode> value,
			uint32_t line, uint32_t column
		) : ExpressionNode(ExprKind::Ref, line, column), value(std::move(value)) {
		}
	};

	struct DereferenceNode : ExpressionNode {
		std::unique_ptr<ExpressionNode> value;

		DereferenceNode(
			std::unique_ptr<ExpressionNode> value,
			uint32_t line, uint32_t column
		) : ExpressionNode(ExprKind::Deref, line, column), value(std::move(value)) {
		}
	};
}
