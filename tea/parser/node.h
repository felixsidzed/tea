#pragma once

#include <vector>
#include <string>
#include <memory>

#include "tea/parser/types.h"

namespace tea {
	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		uint32_t line = 0;
		uint32_t column = 0;

		Node() = default;
		Node(enum NodeType type, uint32_t line = 0, uint32_t column = 0) : type(type), line(line), column(column) {};
	};

	typedef vector<std::unique_ptr<Node>> Tree;

	struct UsingNode : Node {
		string name;

		UsingNode(const string& name) : name(name) {}
	};

	struct FunctionNode : Node {
		Tree body;
		
		vector<LLVMAttributeKind> attrs;
		enum StorageType storage;
		enum CallConv cc;
		string name;
		vector<std::pair<Type, string>> args;
		bool vararg;
		Type returnType;
		map<struct VariableNode*, LLVMValueRef> prealloc;

		FunctionNode(enum StorageType storage, enum CallConv cc, const string& name, const vector<std::pair<Type, string>>& args, Type& returnType, bool vararg)
			: storage(storage), cc(cc), name(name), args(args), returnType(returnType), vararg(vararg) {};
	};

	struct ExpressionNode : Node {
		enum ExpressionType etype;

		string value;

		std::unique_ptr<ExpressionNode> left;
		std::unique_ptr<ExpressionNode> right;

		ExpressionNode(enum ExpressionType etype, const string& value, std::unique_ptr<ExpressionNode> left = nullptr, std::unique_ptr<ExpressionNode> right = nullptr) :
			Node(NODE_ExpressionNode), etype(etype), value(value), left(std::move(left)), right(std::move(right)) { };
	};

	struct ReturnNode : Node {
		std::unique_ptr<ExpressionNode> value;

		ReturnNode(std::unique_ptr<ExpressionNode> value) : value(std::move(value)) {};
	};

	struct CallNode : ExpressionNode {
		vector<string> scope;
		// callee is stored in ExpressionNode::value
		vector<std::unique_ptr<ExpressionNode>> args;

		CallNode(const vector<string>& scope, const string& callee, vector<std::unique_ptr<ExpressionNode>>&& args) :
			scope(scope), args(std::move(args)), ExpressionNode(EXPR_CALL, callee) {}
	};

	struct VariableNode : Node {
		string name;
		Type dataType;
		std::unique_ptr<ExpressionNode> value;

		VariableNode(const string& name, Type dataType, std::unique_ptr<ExpressionNode> value)
			: name(name), dataType(dataType), value(std::move(value)) {}
	};

	struct FunctionImportNode : Node {
		enum CallConv cc;
		string name;
		vector<std::pair<Type, string>> args;
		Type returnType;
		bool vararg;

		FunctionImportNode(enum CallConv cc, const string& name, const vector<std::pair<Type, string>>& args, Type returnType, bool vararg)
			: cc(cc), name(name), args(args), returnType(returnType), vararg(vararg) {
		};
	};

	struct ElseIfNode : Node {
		Tree body;

		std::unique_ptr<struct ElseIfNode> next;
		std::unique_ptr<ExpressionNode> pred;

		ElseIfNode(std::unique_ptr<ExpressionNode> pred) : pred(std::move(pred)) {};
	};

	struct ElseNode : Node {
		Tree body;

		ElseNode() {}
	};

	struct IfNode : Node {
		Tree body;

		std::unique_ptr<ExpressionNode> pred;
		std::unique_ptr<ElseNode> else_;
		std::unique_ptr<ElseIfNode> elseIf;

		IfNode(std::unique_ptr<ExpressionNode> pred) : pred(std::move(pred)) {};
	};

	struct GlobalVariableNode : Node {
		enum StorageType storage;
		string name;
		Type dataType;
		std::unique_ptr<ExpressionNode> value;

		GlobalVariableNode(enum StorageType storage, const string& name, Type dataType, std::unique_ptr<ExpressionNode> value) :
			storage(storage), name(name), dataType(dataType), value(std::move(value)) {};
	};

	struct AssignmentNode : Node {
		std::unique_ptr<ExpressionNode> left;
		std::unique_ptr<ExpressionNode> right;
		enum TokenType extra;

		AssignmentNode(std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right, enum TokenType extra) :
			left(std::move(left)), right(std::move(right)), extra(extra) {}
	};

	struct WhileLoopNode : Node {
		Tree body;
		std::unique_ptr<ExpressionNode> pred;

		WhileLoopNode(std::unique_ptr<ExpressionNode> pred) : pred(std::move(pred)) {};
	};
}
