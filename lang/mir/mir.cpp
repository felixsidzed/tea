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

	BasicBlock* Function::appendBlock(const tea::string& name) {
		return blocks.emplace(scope.add(name), this);
	}

} // namespace tea::mir
