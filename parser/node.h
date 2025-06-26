#pragma once

#include <vector>
#include <string>
#include <memory>

namespace tea {
	enum NodeType : uint8_t {
		NODE_UsingNode,
		NODE_FunctionNode,
		NODE_ReturnNode
	};

	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		Node() = default;
		virtual ~Node() = default;
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
		EXPR_IDENTF
	};

	struct ExpressionNode : Node {
		enum ExpressionType type;

		std::string value;

		ExpressionNode* left;
		ExpressionNode* right;

		ExpressionNode(enum ExpressionType type, const std::string& value, ExpressionNode* left = nullptr, ExpressionNode* right = nullptr) : type(type), value(value), left(left), right(right) {};
	};

	struct ReturnNode : Node {
		std::unique_ptr<ExpressionNode> value;

		ReturnNode(std::unique_ptr<ExpressionNode> value) : value(std::move(value)) {};
	};
}
