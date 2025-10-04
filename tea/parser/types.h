#pragma once

#include <llvm-c/Core.h>

#include "tea/map.h"
#include "tea/string.h"

#ifndef DEFAULT_CC
#define DEFAULT_CC CC_AUTO
#endif

#ifndef tnode
#define tnode(T) NODE_##T
#endif

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
		EXPR_CAST
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

		ATTR__COUNT
	};

	class Type {
	public:
		enum Kind : uint8_t {
			/* ORDER BASIC_TYPE */
			VOID_,
			BOOL,
			CHAR,
			FLOAT,
			INT,
			STRING,
			DOUBLE,
			LONG,
		};

		static vector<LLVMTypeRef> convert;

		LLVMTypeRef llvm;
		bool constant;
		bool sign;

		Type() : llvm(nullptr), constant(false), sign(true) {};
		Type(LLVMTypeRef llvm, bool constant = false, bool sign = true) : llvm(llvm), constant(constant), sign(sign) {}

		static std::pair<Type, bool> get(const string& name);
		static Type get(enum Kind kind, bool constant = false, bool sign = true) { return Type(convert[kind], constant, sign); }

		static void create(const string& name, LLVMTypeRef type);

		bool operator==(const Type& other) const {
			return llvm == other.llvm && constant == other.constant && sign == other.sign;
		}

		bool operator==(LLVMTypeRef other) const {
			return llvm == other;
		}

	private:
		static map<string, enum Kind> name2kind;
	};
}
