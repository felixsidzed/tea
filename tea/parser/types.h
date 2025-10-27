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
