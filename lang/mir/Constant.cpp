#include "mir.h"

namespace tea::mir {

	ConstantNumber* ConstantNumber::get(Module* module, uint64_t val, uint8_t width, bool sign) {
		auto& types = module->ctx.types;

		Type* type = nullptr;
		switch (width) {
		case 1: type = types.Bool(false); break;
		case 8: type = types.Char(false); break;
		case 16: type = types.Short(false); break;
		case 32: type = types.Int(false); break;
		case 64: type = types.Long(false); break;
		default: return nullptr;
		}

		if (val == 0 || val == 1) {
			auto& map = val == 0 ? module->num0Const : module->num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			size_t h = 14695981039346656037uLL;
			h = (h ^ val) * 1099511628211uLL;
			h = (h ^ width) * 1099511628211uLL;

			auto& entry = module->numConst[h];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	template<typename T, typename>
	ConstantNumber* ConstantNumber::get(Module* module, double fval, uint8_t width, bool sign) {
		auto& types = module->ctx.types;

		Type* type = nullptr;
		switch (width) {
		case 32: type = types.Float(false); break;
		case 64: type = types.Double(false); break;
		default: return nullptr;
		}

		uint64_t val = *(uint64_t*)&fval;

		std::unique_ptr<ConstantNumber>* entry = nullptr;
		if (val == 0 || val == 1) {
			auto& map = val == 0 ? module->num0Const : module->num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			auto& entry = module->numConst[val];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	template ConstantNumber* ConstantNumber::get<double>(Module*, double, uint8_t, bool);

	ConstantString* ConstantString::get(Module* module, const tea::string& val) {
		auto& entry = module->strConst[val];
		if (!entry)
			entry.reset(new ConstantString(module->ctx, val));
		return entry.get();
	}

	ConstantArray* ConstantArray::get(Module* module, Type* elementType, Value** values, uint32_t n) {
		uint64_t h = 1469598103934665603uLL;
		for (size_t i = 0; i < n; i++)
			h = (h ^ (uintptr_t)values[i]) * 1099511628211uLL;

		auto& entry = module->arrConst[h];
		if (!entry)
			entry.reset(new ConstantArray(module->ctx, elementType, values, n));
		return entry.get();
	}

	ConstantPointer* ConstantPointer::get(Module* module, Type* pointee, uintptr_t value) {
		uint64_t h = 1469598103934665603uLL;
		h = (h ^ (uintptr_t)pointee) * 1099511628211uLL;
		h = (h ^ value) * 1099511628211uLL;

		auto& entry = module->ptrConst[h];
		if (!entry)
			entry.reset(new ConstantPointer(module->ctx, pointee, value));
		return entry.get();
	}

} // namespace tea::mir
