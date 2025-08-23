#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <llvm-c/Core.h>

#ifndef DEFAULT_CC
#ifdef _WIN32
#define DEFAULT_CC CC_FAST
#else
#define DEFAULT_CC CC_C
#endif
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
	};

	enum CallingConvention : uint8_t {
		CC_C,
		CC_FAST,
		CC_STD,

		CC__COUNT
	};

	enum Attribute : uint8_t {
		ATTR_INLINE,
		ATTR_NORETURN,

		ATTR__COUNT
	};

	class Type {
	public:
		enum Kind : uint8_t {
			/* ORDER BASIC_TYPE */
			INT,
			FLOAT,
			DOUBLE,
			CHAR,
			STRING,
			VOID_,
			BOOL,
			LONG
		};

		static std::vector<LLVMTypeRef> convert;

		LLVMTypeRef llvm;
		bool constant;

		Type() : llvm(nullptr), constant(false) {};
		Type(LLVMTypeRef llvm, bool constant = false) : llvm(llvm), constant(constant) {}

		static Type get(enum Kind kind) { return Type(convert[kind]); }
		static std::pair<Type, bool> get(const std::string& name);

		static void create(const std::string& name, LLVMTypeRef type);

		bool operator==(const Type& other) const {
			return LLVMGetTypeKind(llvm) == LLVMGetTypeKind(other.llvm) && constant == other.constant;
		}

		const char* str() {

		}

	private:
		static std::unordered_map<std::string, Kind> name2kind;
	};
}
