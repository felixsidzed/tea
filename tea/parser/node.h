#pragma once

#include <vector>
#include <string>
#include <memory>

#ifdef _WIN32
#define DEFAULT_CC CC_FAST
#else
#define DEFAULT_CC CC_C
#endif

#define tnode(T) NODE_##T

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
		tnode(UsingNode),
		tnode(FunctionNode),
		tnode(ExpressionNode),
		tnode(ReturnNode),
		tnode(CallNode),
		tnode(VariableNode),
		tnode(FunctionImportNode),
		tnode(IfNode),
		tnode(ElseNode),
		tnode(ElseIfNode)
	};

	enum StorageType : uint8_t {
		STORAGE_PUBLIC,
		STORAGE_PRIVATE
	};

	enum ExpressionType : uint8_t {
		EXPR_STRING,

		EXPR_INT,
		EXPR_FLOAT,
		EXPR_DOUBLE,

		EXPR_IDENTF,
		EXPR_CALL,

		EXPR_ADD,
		EXPR_SUB,
		EXPR_MUL,
		EXPR_DIV,

		EXPR_EQ,
		EXPR_NEQ,
		EXPR_LT,
		EXPR_GT,
		EXPR_LE,
		EXPR_GE,

		EXPR_NOT,
		EXPR_AND,
		EXPR_OR,
	};

	enum CallingConvention : uint8_t {
		CC_C,
		CC_FAST,
		CC_STD,

		CC__COUNT
	};

	enum Attribute : uint8_t {
		ATTR_INLINE,

		ATTR__COUNT
	};

	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		uint32_t line = 0;
		uint32_t column = 0;

		Node() = default;
		Node(enum NodeType type, uint32_t line = 0, uint32_t column = 0) : type(type), line(line), column(column) {};
	};

	typedef std::vector<std::unique_ptr<Node>> Tree;

	struct UsingNode : Node {
		std::string name;

		UsingNode(const std::string& name) : name(name) {}
	};

	struct FunctionNode : Node {
		Tree body;
		
		std::vector<enum Attribute> attrs;
		enum StorageType storage;
		enum CallingConvention cc;
		std::string name;
		std::vector<std::pair<enum Type, std::string>> args;
		enum Type returnType;

		FunctionNode(enum StorageType storage, enum CallingConvention cc, const std::string& name, const std::vector<std::pair<enum Type, std::string>>& args, enum Type returnType)
			: storage(storage), cc(cc), name(name), args(args), returnType(returnType) {};
	};

	struct ExpressionNode : Node {
		enum ExpressionType etype;

		std::string value;

		std::unique_ptr<ExpressionNode> left;
		std::unique_ptr<ExpressionNode> right;

		ExpressionNode(enum ExpressionType etype, const std::string& value, std::unique_ptr<ExpressionNode> left = nullptr, std::unique_ptr<ExpressionNode> right = nullptr) :
			Node(NODE_ExpressionNode), etype(etype), value(value), left(std::move(left)), right(std::move(right)) { };
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

	struct VariableNode : Node {
		std::string name;
		enum Type dataType;
		std::unique_ptr<ExpressionNode> value;

		VariableNode(const std::string& name, enum Type dataType, std::unique_ptr<ExpressionNode> value)
			: name(name), dataType(dataType), value(std::move(value)) {}
	};

	struct FunctionImportNode : Node {
		enum CallingConvention cc;
		std::string name;
		std::vector<std::pair<enum Type, std::string>> args;
		enum Type returnType;

		FunctionImportNode(enum CallingConvention cc, const std::string& name, const std::vector<std::pair<enum Type, std::string>>& args, enum Type returnType)
			: cc(cc), name(name), args(args), returnType(returnType) {
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
}
