#pragma once

#include "core/map.h"
#include "core/string.h"
#include "core/vector.h"

#include <memory>

namespace tea {

	enum class TypeKind : uint8_t {
		Void, Bool, Char, Short, Float, Int, Double, Long, String,
		Pointer, Function, Array, Struct
	};

	struct Type;
	struct ArrayType;
	struct StructType;
	struct PointerType;
	struct FunctionType;

	class TypeTable {
	public:
		tea::vector<std::unique_ptr<Type>> types;
		tea::map<size_t, std::unique_ptr<StructType>> structTypes;
		tea::map<size_t, std::unique_ptr<FunctionType>> funcTypes;

		Type* get(const tea::string& name);

		PointerType* Pointer(Type* pointee, bool constant = false);
		ArrayType* Array(Type* elementType, uint32_t size, bool constant = false);
		StructType* Struct(Type** body, uint32_t n, const char* name, bool packed = false);
		FunctionType* Function(Type* returnType, const tea::vector<Type*>& params, bool vararg = false);

		Type* Void(bool constant = false);
		Type* Bool(bool constant = false);
		Type* Float(bool constant = false);
		Type* Double(bool constant = false);
		Type* String(bool constant = false);
		Type* Int(bool constant = false, bool sign = true);
		Type* Long(bool constant = false, bool sign = true);
		Type* Char(bool constant = false, bool sign = true);
		Type* Short(bool constant = false, bool sign = true);

	private:
		Type* primitive(TypeKind kind, bool constant = false, bool sign = true);

		template<typename T, typename... Args>
		T* allocate(Args&&... args) {
			auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
			T* result = ptr.get();
			types.emplace(std::move(ptr));
			return result;
		}
	};

	struct Type {
		TypeKind kind;
		bool constant;
		bool sign;
		uint32_t extra;

		Type(TypeKind kind = TypeKind::Void, bool constant = false, bool sign = true)
			: kind(kind), constant(constant), sign(sign), extra(0) {
		}

		virtual ~Type() = default;

		tea::string str();

		bool isFloat() const;
		bool isNumeric() const;
		bool isIndexable() const;

		Type* getElementType() const;
		bool equals(const Type* other) const;
	};

	struct PointerType : Type {
		Type* pointee;

		PointerType(Type* pointee, bool constant = false)
			: Type(TypeKind::Pointer, constant), pointee(pointee) {
		}
	};

	struct FunctionType : Type {
		Type* returnType;
		tea::vector<Type*> params;

		FunctionType(Type* returnType, const tea::vector<Type*>& params, bool vararg = false)
			: Type(TypeKind::Function),
			returnType(returnType),
			params(params) {
			extra = vararg;
		}
	};


	struct ArrayType : Type {
		Type* elementType;

		ArrayType(Type* elementType, uint32_t size, bool constant = false)
			: Type(TypeKind::Array, constant),
			elementType(elementType) {
			extra = size;
		}
	};

	struct StructType : Type {
		const char* name;
		tea::vector<Type*> body;

		StructType(Type** body, uint32_t n, const char* name, bool packed)
			: Type(TypeKind::Struct),
			body(body, n),
			name(name) {
			extra = packed;
		}
	};
}
