#include "mir.h"

#include "frontend/parser/AST.h"

namespace tea::mir {

	Function* Module::addFunction(const tea::string& name, tea::FunctionType* ftype) {
		Function* f = (Function*)body.emplace(std::make_unique<Function>(StorageClass::Public, ftype, this))->get();
		f->name = scope.add(name);

		uint32_t i = 1;
		for (tea::Type* paramType : ftype->params) {
			auto param = std::make_unique<Value>(ValueKind::Parameter, paramType);
			param->name = scope.add(std::format("arg{}", i).c_str());
			f->params.emplace(std::move(param));

			i++;
		}

		return f;
	}

	Global* Module::addGlobal(const tea::string& name, Type* type, Value* initializer) {
		Global* g = (Global*)body.emplace_front(std::make_unique<Global>(Type::Pointer(type), StorageClass::Public, initializer))->get();
		g->name = scope.add(name);
		return g;
	}

	uint32_t Module::getSize(const tea::Type* type) {
		switch (type->kind) {
		case TypeKind::Char:
		case TypeKind::Bool:
			return 1;

		case TypeKind::Short:
			return 2;

		case TypeKind::Int:
		case TypeKind::Float:
			return 4;

		case TypeKind::Long:
		case TypeKind::Double:
			return 8;

		case TypeKind::Pointer:
		case TypeKind::String:
		case TypeKind::Function:
			return dl.maxNativeBytes;

		case TypeKind::Array: {
			ArrayType* arr = (ArrayType*)type;
			return getSize(arr->elementType) * arr->size;
		}

		case TypeKind::Struct: {
			StructType* st = (StructType*)type;
			if (!st->body.size)
				return 1;

			uint32_t size = 0;
			uint32_t alignment = 0;

			if (!st->packed) {
				for (const auto& el : st->body) {
					uint32_t elSize = getSize(el);
					if (elSize > alignment)
						alignment = elSize - 1;
				}
			}
		
			for (const auto& el : st->body)
				size += getSize(el);
			
			return (size + alignment) & ~alignment;
		}

		default:
			return 0;
		}
	}

	BasicBlock* Function::appendBlock(const tea::string& name) {
		return blocks.emplace(scope.add(name), this);
	}

} // namespace tea::mir
