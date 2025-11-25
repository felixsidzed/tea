#pragma once

#include "common/string.h"
#include "common/vector.h"

namespace tea {

	namespace mir {
		class Context;
	}

	enum class TypeKind : uint32_t {
		// Primitive
		Void, Bool, Char, Short, Float, Int, Double, Long, String,

		// Complex
		Pointer, Function, Array, Struct
	};

	struct ArrayType;
	struct StructType;
	struct PointerType;
	struct FunctionType;

	struct Type {
		TypeKind kind : 30;
		uint32_t constant : 1;
		uint32_t sign : 1;

		Type()
			: kind(TypeKind::Void), constant(false), sign(true) {
		}

		explicit Type(TypeKind b, bool constant = false, bool sign = true)
			: kind(b), constant(constant), sign(sign) {
		}

		tea::string str();

		static Type* get(const tea::string& name);

		static Type* Void(bool constant = false, mir::Context* ctx = nullptr);
		static Type* Bool(bool constant = false, mir::Context* ctx = nullptr);
		static Type* String(bool constant = false, mir::Context* ctx = nullptr);
		static Type* Int(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);
		static Type* Long(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);
		static Type* Char(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);
		static Type* Short(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);
		static Type* Float(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);
		static Type* Double(bool constant = false, bool sign = true, mir::Context* ctx = nullptr);

		static PointerType* Pointer(Type* pointee, bool constant = false, mir::Context* ctx = nullptr);
		static FunctionType* Function(Type* returnType, const tea::vector<Type*>& params, bool vararg = false, mir::Context* ctx = nullptr);
		static FunctionType* Function(Type* returnType, bool vararg = false, mir::Context* ctx = nullptr) {
			return Type::Function(returnType, {}, vararg, ctx);
		}
		static ArrayType* Array(Type* elementType, uint32_t size, bool constant = false, mir::Context* ctx = nullptr);
		static StructType* Struct(Type** body, uint32_t n, const char* name, bool packed = false, mir::Context* ctx = nullptr);

		bool isNumeric() {
			return kind == TypeKind::Bool || kind == TypeKind::Char || kind == TypeKind::Short || kind == TypeKind::Int || kind == TypeKind::Long;
		}

		bool isFloat() {
			return kind == TypeKind::Float || kind == TypeKind::Double;
		}
		
		bool isIndexable() {
			return kind == TypeKind::Array || kind == TypeKind::Pointer;
		}

		Type* getElementType() {
			switch (kind) {
			case TypeKind::Array:
				return *(Type**)((char*)this + sizeof(Type) + sizeof(uint32_t));
			case TypeKind::Pointer:
				return *(Type**)((char*)this + sizeof(Type));
			default:
				return nullptr;
			}
		}
	};

	struct PointerType : Type {
		Type* pointee;

		PointerType(Type* pointee, bool constant = false)
			: Type(TypeKind::Pointer, constant), pointee(pointee) {
		}
	};

	struct FunctionType : Type {
		bool vararg;
		Type* returnType;
		tea::vector<Type*> params;

		FunctionType(Type* returnType, const tea::vector<Type*>& params, bool vararg = false)
			: Type(TypeKind::Function, false), returnType(returnType), params(params), vararg(vararg) {
		}
	};

	struct ArrayType : Type {
		uint32_t size;
		Type* elementType;

		ArrayType(Type* elementType, uint32_t size, bool constant = false)
			: Type(TypeKind::Array, constant), elementType(elementType), size(size) {
		}
	};

	struct StructType : Type {
		bool packed;
		const char* name;
		tea::vector<Type*> body;

		StructType(Type** body, uint32_t n, const char* name, bool packed)
			: Type(TypeKind::Struct), packed(packed), body(body, n), name(name) {
		}
	};

} // namespace tea
