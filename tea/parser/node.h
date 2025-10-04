#pragma once

#include <vector>
#include <string>
#include <memory>

namespace tea {
	enum NodeType : uint8_t {
		NODE_UsingNode,
		NODE_FunctionNode,
		NODE_ExpressionNode,
		NODE_ReturnNode,
		NODE_CallNode,
	};

	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		Node() = default;
		virtual ~Node() = default;

		Node(enum NodeType type) : type(type) {};
	};

	typedef std::vector<std::unique_ptr<Node>> Tree;

	struct UsingNode : Node {
		std::string name;

		UsingNode(const std::string& name) : name(name) {}
	};

	struct FunctionNode : Node {
		Tree body;

		uint8_t storage;

		FunctionNode(uint8_t storage) : storage(storage) {};
	};

	enum ExpressionType : uint8_t {
		EXPR_INT,
		EXPR_FLOAT,
		EXPR_STRING,
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
