#include "mir.h"

namespace tea::mir {

	ConstantNumber* ConstantNumber::get(uint64_t val, uint8_t width, bool sign, Context* ctx) {
		if (!ctx) ctx = getGlobalContext();

		Type* type = nullptr;
		switch (width) {
		case 1:
			type = Type::Bool(false, ctx);
			break;
		case 8:
			type = Type::Char(false, sign, ctx);
			break;
		case 16:
			type = Type::Short(false, sign, ctx);
			break;
		case 32:
			type = Type::Int(false, sign, ctx);
			break;
		case 64:
			type = Type::Long(false, sign, ctx);
			break;
		default:
			return nullptr;
		}

		std::unique_ptr<ConstantNumber>* entry = nullptr;
		if (val == 0 || val == 1) {
			auto& map = val == 0 ? ctx->num0Const : ctx->num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			auto& entry = ctx->numConst[val];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	ConstantString* ConstantString::get(const tea::string& val, Context* ctx) {
		if (!ctx) ctx = getGlobalContext();

		auto& entry = ctx->strConst[val];
		if (!entry)
			entry.reset(new ConstantString(val));
		return entry.get();
	}

	ConstantArray* ConstantArray::get(Type* elementType, Value** values, uint32_t n, Context* ctx) {
		if (!ctx) ctx = getGlobalContext();

		uint64_t h = 1469598103934665603uLL;
		for (size_t i = 0; i < n; i++)
			h = (h ^ (uintptr_t)values[i]) * 1099511628211uLL;

		auto& entry = ctx->arrConst[h];
		if (!entry)
			entry.reset(new ConstantArray(elementType, values, n));
		return entry.get();
	}

} // namespace tea::mir
