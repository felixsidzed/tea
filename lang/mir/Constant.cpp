#include "mir.h"

namespace tea::mir {

	ConstantNumber* Module::constnum(uint64_t val, uint8_t width, bool sign) {
		auto& types = ctx.types;

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
			auto& map = val == 0 ? num0Const : num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			size_t h = 14695981039346656037uLL;
			h = (h ^ val) * 1099511628211uLL;
			h = (h ^ width) * 1099511628211uLL;

			auto& entry = numConst[h];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	template<typename T, typename>
	ConstantNumber* Module::constnum(double fval, uint8_t width, bool sign) {
		auto& types = ctx.types;

		Type* type = nullptr;
		switch (width) {
		case 32: type = types.Float(false); break;
		case 64: type = types.Double(false); break;
		default: return nullptr;
		}

		uint64_t val = *(uint64_t*)&fval;

		std::unique_ptr<ConstantNumber>* entry = nullptr;
		if (val == 0 || val == 1) {
			auto& map = val == 0 ? num0Const : num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			auto& entry = numConst[val];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	template ConstantNumber* Module::constnum<double>(double, uint8_t, bool);

	ConstantString* Module::conststr(const tea::string& val) {
		auto& entry = strConst[val];
		if (!entry)
			entry.reset(new ConstantString(ctx, val));
		return entry.get();
	}

	ConstantArray* Module::constarr(Type* elementType, Value** values, uint32_t n) {
		ConstantArrayKey key = { elementType, { values, n } };
		auto& entry = arrConst[key];
		if (!entry)
			entry.reset(new ConstantArray(ctx, elementType, values, n));
		return entry.get();
	}

	ConstantPointer* Module::constptr(Type* pointee, uintptr_t value) {
		uint64_t h = 1469598103934665603uLL;
		h = (h ^ (uintptr_t)pointee) * 1099511628211uLL;
		h = (h ^ value) * 1099511628211uLL;

		auto& entry = ptrConst[h];
		if (!entry)
			entry.reset(new ConstantPointer(ctx, pointee, value));
		return entry.get();
	}

} // namespace tea::mir
