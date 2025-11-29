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
		Call,
		FunctionImport,
		ModuleImport,
		Variable
	};

	enum class ExprKind : uint32_t {
		String, Char, Int, Float, Double,
		Identf, Call,
		Add, Sub, Mul, Div/*,
		Eq, Neq, Lt, Gt, Le, Ge,
		Not, And, Or,
		Ref, Deref, Cast,
		Array, Index,
		Band, Bor, Bxor, Shr, Shl*/
	};

	enum class StorageClass {
		Public, Private
	};

	enum class CallingConvention {
		Std, Fast, C, Auto
	};

	struct FunctionExtra {
		uint32_t vararg : 1;
		uint32_t cc : 2;
		uint32_t storage : 2;
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
		tea::string name;
		tea::vector<std::pair<Type*, tea::string>> params;
		Type* returnType;

		Tree body;

		FunctionNode(
			StorageClass vis,
			CallingConvention cc,
			const tea::string& name,
			const tea::vector<std::pair<Type*, tea::string>>& params,
			Type* retType,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::Function, line, column),
			name(name), params(params), returnType(retType) {
			setVisibility(vis);
			setCC(cc);
		}

		bool isVarArg() const { return ((FunctionExtra*)&extra)->vararg; }
		void setVarArg(bool vararg) { ((FunctionExtra*)&extra)->vararg = vararg; }

		StorageClass getVisibility() const { return (StorageClass)((FunctionExtra*)&extra)->storage; }
		void setVisibility(StorageClass vis) { ((FunctionExtra*)&extra)->storage = (uint32_t)vis; }

		CallingConvention getCC() const { return (CallingConvention)((FunctionExtra*)&extra)->cc; }
		void setCC(CallingConvention cc) { ((FunctionExtra*)&extra)->cc = (uint32_t)cc; };
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
		tea::string name;
		tea::vector<std::pair<Type*, tea::string>> params;
		Type* returnType;

		FunctionImportNode(
			CallingConvention cc,
			const tea::string& name,
			const tea::vector<std::pair<Type*, tea::string>>& params,
			Type* retType,
			uint32_t line, uint32_t column
		)
			: Node(NodeKind::FunctionImport, line, column),
			name(name), params(params), returnType(retType), cc(cc) {
			setCC(cc);
		}

		bool isVarArg() const { return ((FunctionExtra*)&extra)->vararg; }
		void setVarArg(bool vararg) { ((FunctionExtra*)&extra)->vararg = vararg; }

		CallingConvention getCC() const { return (CallingConvention)((FunctionExtra*)&extra)->cc; }
		void setCC(CallingConvention cc) { ((FunctionExtra*)&extra)->cc = (uint32_t)cc; };
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
}
