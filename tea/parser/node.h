#pragma once

#include <vector>
#include <string>
#include <memory>

#include "tea/parser/types.h"

namespace tea {
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
		tnode(ElseIfNode),
		tnode(GlobalVariableNode),
		tnode(AssignmentNode),
		tnode(WhileLoopNode),
		tnode(ForLoopNode),
		tnode(LoopInterruptNode),
		tnode(ObjectNode),
	};

	enum StorageType : uint8_t {
		STORAGE_PUBLIC,
		STORAGE_PRIVATE
	};

	enum ExpressionType : uint8_t {
		EXPR_STRING,
		EXPR_CHAR,
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

		EXPR_REF,
		EXPR_DEREF,
		EXPR_CAST,

		EXPR_ARRAY,
		EXPR_INDEX
	};

	enum CallConv : uint8_t {
		CC_C,
		CC_STD,
		CC_FAST,
		CC_AUTO,

		CC__COUNT
	};

	enum Attribute : uint8_t {
		ATTR_INLINE,
		ATTR_NORETURN,

		ATTR__LLVM_COUNT,

		ATTR_NONAMESPACE,

		ATTR__COUNT
	};

	struct Node {
		enum NodeType type = (enum NodeType)0;

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
		
		vector<enum Attribute> attrs;
		enum StorageType storage;
		enum CallConv cc;
		bool vararg;
		string name;
		vector<std::pair<Type, string>> args;
		Type returnType;
		map<struct VariableNode*, LLVMValueRef> prealloc;

		FunctionNode(enum StorageType storage, enum CallConv cc, const string& name, const vector<std::pair<Type, string>>& args, const Type& returnType, bool vararg)
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
		std::unique_ptr<ExpressionNode> callee;
		vector<std::unique_ptr<ExpressionNode>> args;

		CallNode(std::unique_ptr<ExpressionNode> callee, vector<std::unique_ptr<ExpressionNode>>&& args) : args(std::move(args)), callee(std::move(callee)), ExpressionNode(EXPR_CALL, "") { }
	};

	struct VariableNode : Node {
		string name;
		Type dataType;
		std::unique_ptr<ExpressionNode> value;

		VariableNode(const string& name, Type dataType, std::unique_ptr<ExpressionNode> value)
			: name(name), dataType(dataType), value(std::move(value)) {}
	};

	struct FunctionImportNode : Node {
		vector<enum Attribute> attrs;
		enum CallConv cc;
		bool vararg;
		string name;
		vector<std::pair<Type, string>> args;
		Type returnType;

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

	// ExpressionNode::value is used to indicate whether the index is an array index (`array[123]') or an object index (`object.field' / `object->field')
	struct IndexNode : ExpressionNode {
		IndexNode(std::unique_ptr<ExpressionNode> val, std::unique_ptr<ExpressionNode> idx, uint8_t kind) : ExpressionNode(EXPR_INDEX, kind, std::move(val), std::move(idx)) {};
	};

	struct ArrayNode : ExpressionNode {
		vector<std::unique_ptr<ExpressionNode>> init;

		ArrayNode(vector<std::unique_ptr<ExpressionNode>> init) : init(std::move(init)), ExpressionNode(EXPR_ARRAY, "") {};
	};

	struct ForLoopNode : Node {
		Tree body;

		vector<VariableNode*> vars;
		std::unique_ptr<ExpressionNode> pred;
		std::unique_ptr<AssignmentNode> step;

		ForLoopNode(vector<VariableNode*> vars, std::unique_ptr<ExpressionNode> pred, std::unique_ptr<AssignmentNode> step) : vars(vars), pred(std::move(pred)), step(std::move(step)) {};
	};

	struct LoopInterruptNode : Node {
		bool exit;

		LoopInterruptNode(bool exit) : exit(exit) {};
	};

	struct ObjectNode : Node {
		string name;
		vector<std::unique_ptr<FunctionNode>> methods;
		vector<std::unique_ptr<GlobalVariableNode>> fields;

		ObjectNode(const string& name, vector<std::unique_ptr<GlobalVariableNode>> fields, vector<std::unique_ptr<FunctionNode>> methods) : name(name), fields(std::move(fields)), methods(std::move(methods)) {};
	};
}
