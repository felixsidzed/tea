#pragma once

#include <vector>
#include <string>
#include <memory>

namespace tea {
	enum Type : uint8_t {
		TYPE_INT,
		TYPE_FLOAT,
		TYPE_DOUBLE,
		TYPE_CHAR,
		TYPE_STRING,
		TYPE_VOID,
		TYPE_BOOL,

		TYPE__COUNT
	};

	enum NodeType : uint8_t {
		NODE_UsingNode,
		NODE_FunctionNode,
		NODE_ExpressionNode,
		NODE_ReturnNode,
		NODE_CallNode,
	};

	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		uint32_t line = 0;
		uint32_t column = 0;

		Node() = default;
		virtual ~Node() = default;

		Node(enum NodeType type, uint32_t line = 0, uint32_t column = 0) : type(type), line(line), column(column) {};
	};

	typedef std::vector<std::unique_ptr<Node>> Tree;

	struct UsingNode : Node {
		std::string name;

		UsingNode(const std::string& name) : name(name) {}
	};

	enum StorageType : uint8_t {
		STORAGE_PUBLIC,
		STORAGE_PRIVATE
	};

	struct FunctionNode : Node {
		Tree body;
		
		enum StorageType storage;
		std::string name;
		enum Type returnType;

		FunctionNode(enum StorageType storage, const std::string& name, enum Type returnType) : storage(storage), name(name), returnType(returnType) {};
	};

	enum ExpressionType : uint8_t {
		EXPR_STRING,

		EXPR_INT,
		EXPR_FLOAT,
		EXPR_DOUBLE,

		EXPR_IDENTF,

		EXPR_CALL
	};

	struct ExpressionNode : Node {
		enum ExpressionType etype;

		std::string value;

		ExpressionNode* left;
		ExpressionNode* right;

		ExpressionNode(enum ExpressionType etype, const std::string& value, ExpressionNode* left = nullptr, ExpressionNode* right = nullptr) :
			Node(NODE_ExpressionNode), etype(etype), value(value), left(left), right(right) { };
	};

	struct ReturnNode : Node {
		std::unique_ptr<ExpressionNode> value;

		ReturnNode(std::unique_ptr<ExpressionNode> value) : value(std::move(value)) {};
	};

	struct CallNode : ExpressionNode {
		std::vector<std::string> scope;
		// the callee is stored in ExpressionNode::value
		std::vector<std::unique_ptr<ExpressionNode>> args;

		CallNode(const std::vector<std::string>& scope, const std::string& callee, std::vector<std::unique_ptr<ExpressionNode>>&& args) :
			scope(scope), args(std::move(args)), ExpressionNode(EXPR_CALL, callee) {}
	};
}
