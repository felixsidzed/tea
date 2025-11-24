#pragma once

#include <memory>

#include "common/Type.h"
#include "common/string.h"
#include "common/vector.h"

#include "frontend/parser/AST.h"

#include "mir/Scope.h"
#include "mir/Context.h"

namespace tea::mir {

	enum class OpCode {
		// Arithmetic operations
		Add, Sub, Mul, Div, Mod,

		// Bitwise operations
		Not, And, Or, Xor, Shl, Shr,

		// Comparison
		ICmp, FCmp,

		// Memory
		Load, Store, Alloca, GetElementPtr,

		// Control flow
		Br, CondBr, Ret, Phi,

		// Functions
		Call,

		// Miscellaneous
		Nop, Cast, Unreachable
	};

	enum class FCmpPredicate { OEQ, ONEQ, OGT, OGE, OLT, OLE, TRUE, FALSE };
	enum class ICmpPredicate { EQ, NEQ, SGT, UGT, SGE, UGE, SLT, ULT, SLE, ULE };

	enum class ValueKind {
		Function,
		Parameter,
		Constant,
		Global,
		Instruction,

		Null
	};

	enum class FunctionAttribute {
		Inline = 1 << 0,
		NoReturn = 1 << 1,
		NoNamespace = 1 << 2
	};

	enum class GlobalAttribute {
		ThreadLocal = 1 << 0
	};

	enum class StorageClass {
		Public,
		Private
	};

	enum class ConstantKind {
		Number,
		String,
		Array
	};

	struct SourceLoc { uint32_t line, column; };

	class Value {
	protected:
		friend void dump(const tea::mir::Value* value);

		uint32_t subclassData = 0;

	public:
		ValueKind kind;

		tea::Type* type;
		const char* name;

		Value(ValueKind kind, tea::Type* type) : kind(kind), type(type), name(nullptr) {
		}
	};

	struct Instruction {
		OpCode op;
		union {
			ICmpPredicate icmpPred;
			FCmpPredicate fcmpPred;
		};
		tea::vector<Value*> operands;
		Value result;
		SourceLoc loc;

		Instruction()
			: op(OpCode::Nop), icmpPred(ICmpPredicate::EQ), result(ValueKind::Null, nullptr), loc{ .line = 0,.column = 0 } {
		}
	};

	class Function;

	class BasicBlock {
		friend class Builder;
		friend void dump(const Function* func);

		Scope scope;
		tea::vector<Instruction> body;

	public:
		Function* parent = nullptr;
		const char* name = nullptr;

		BasicBlock(const char* name, Function* parent)
			: name(name), parent(parent) {
		}
	};

	class ConstantNumber : public Value {
		union {
			double f;
			uint64_t i;
		} value;

	public:
		ConstantNumber(tea::Type* type, uint64_t i) : Value(ValueKind::Constant, type), value{ .i = i } {
			subclassData = (uint32_t)ConstantKind::Number;
		};

		ConstantNumber(tea::Type* type, double f) : Value(ValueKind::Constant, type), value{ .f = f } {
			subclassData = (uint32_t)ConstantKind::Number;
		};

		uint8_t getBitwidth() {
			switch (type->kind) {
			case TypeKind::Bool:
				return 1;
			case TypeKind::Char:
				return 8;
			case TypeKind::Short:
				return 16;
			case TypeKind::Float:
			case TypeKind::Int:
				return 32;
			case TypeKind::Double:
			case TypeKind::Long:
				return 64;
			default:
				return 0;
			}
		}

		double getDouble() {
			switch (type->kind) {
			case TypeKind::Float:
			case TypeKind::Double:
				return value.f;
			default:
				return 0.0;
			}
		}

		uint64_t getInteger() {
			switch (type->kind) {
			case TypeKind::Bool:
			case TypeKind::Char:
			case TypeKind::Short:
			case TypeKind::Int:
			case TypeKind::Long:
				return value.i;
			default:
				return 0;
			}
		}

		int64_t getSInteger() {
			if (type->kind == TypeKind::Float || type->kind == TypeKind::Double)
				return 0;

			uint8_t bits = getBitwidth();
			uint64_t val = value.i;
			if (bits < 64) {
				if (val & (1ull << (bits - 1)))
					val |= (~0ull << bits);
				else
					val &= ((1ull << bits) - 1);
			}
			return (int64_t)val;
		}

		static ConstantNumber* get(uint64_t value, uint8_t bitwidth, bool sign = true, Context* ctx = nullptr);
	};

	class ConstantString : public Value {
	public:
		tea::string value;

		ConstantString(const tea::string& value) : Value(ValueKind::Constant, Type::Array(Type::Char(true), (uint32_t)value.length(), true)), value(value) {
			subclassData = (uint32_t)ConstantKind::String;
		}

		static ConstantString* get(const tea::string& value, Context* ctx = nullptr);
	};

	class ConstantArray : public Value {
	public:
		tea::vector<Value*> values;

		ConstantArray(Type* elementType, Value** values, uint32_t n)
			: Value(ValueKind::Constant, Type::Array(elementType, n, true)), values(values, n) {
			subclassData = (uint32_t)ConstantKind::Array;
		}

		static ConstantArray* get(Type* elementType, Value** values, uint32_t n, Context* ctx = nullptr);
	};

	class Function : public Value {
		friend class Module;
		friend class Builder;
		friend void dump(const Function* func);

		Scope scope;
		StorageClass storage;
		tea::vector<BasicBlock> blocks;
		tea::vector<std::unique_ptr<Value>> params;
	
	public:
		Module* parent = nullptr;

		Function(StorageClass storage, tea::FunctionType* type, Module* parent)
			: Value(ValueKind::Function, type), storage(storage), parent(parent) {
		};

		void clearAttributes() { subclassData = 0; };
		void addAttribute(FunctionAttribute attr) { subclassData |= (uint32_t)attr; };
		bool hasAttribute(FunctionAttribute attr) { return subclassData & (uint32_t)attr; };
		void removeAttribute(FunctionAttribute attr) { subclassData &= ~(uint32_t)attr; };

		BasicBlock* appendBlock(const tea::string& name);

		Value* getParam(uint32_t i) { return params[i].get(); }
		BasicBlock* getBlock(uint32_t i) { return &blocks[i]; }
	};

	class Global : public Value {
	public:
		Value* initializer = nullptr;
		StorageClass storage = StorageClass::Public;

		Global(Type* type, StorageClass storage, Value* initializer = nullptr) : Value(ValueKind::Global, type), storage(storage), initializer(initializer) {
		}

		void clearAttributes() { subclassData = 0; };
		void addAttribute(GlobalAttribute attr) { subclassData |= (uint32_t)attr; };
		bool hasAttribute(GlobalAttribute attr) { return subclassData & (uint32_t)attr; };
		void removeAttribute(GlobalAttribute attr) { subclassData &= ~(uint32_t)attr; };
	};

	class Module {
		friend void dump(const Module* module);

		Scope scope;
		tea::vector<std::unique_ptr<Value>> body;

	public:
		const tea::string source;

		Module(const tea::string& source) : source(source) {
		}

		Function* addFunction(const tea::string& name, tea::FunctionType* ftype);
		Global* addGlobal(const tea::string& name, Type* type, Value* initializer);
	};

	class Builder {
		BasicBlock* block = nullptr;

	public:
		Builder() {
		};

		void insertInto(BasicBlock* block) { this->block = block; }
		
		Instruction* ret(Value* val);
		Value* load(Value* ptr, const char* name);
		Instruction* store(Value* ptr, Value* val);
		Value* globalString(const tea::string& str);
		Value* alloca_(Type* type, const char* name);
		Value* cast(Value* ptr, Type* targetType, const char* name);
		Value* binop(OpCode op, Value* lhs, Value* rhs, const char* name);
		Value* arithm(OpCode op, Value* lhs, Value* rhs, const char* name);
		Instruction* unreachable();
		Value* gep(Value* ptr, Value* idx, const char* name);
		Value* gep(Value* ptr, Value** indicies, uint32_t n, const char* name);
		Instruction* br(BasicBlock* block, bool change = true);
		Value* call(Value* func, Value* arg, const char* name);
		Value* call(Value* func, Value** args, uint32_t n, const char* name);
	};

} // namespace tea::mir
