#pragma once

#include <memory>

#include "common/map.h"
#include "common/Type.h"

#include "mir/mir.h"

namespace tea::mir {
	class Context {
		friend class ConstantArray;
		friend class ConstantNumber;
		friend class ConstantString;

		tea::vector<std::unique_ptr<tea::Type>> types;

		tea::map<size_t, std::unique_ptr<ConstantArray>> arrConst;
		tea::map<tea::string, std::unique_ptr<ConstantString>> strConst;

		tea::map<uint8_t, std::unique_ptr<ConstantNumber>> num0Const;
		tea::map<uint8_t, std::unique_ptr<ConstantNumber>> num1Const;
		tea::map<uint64_t, std::unique_ptr<ConstantNumber>> numConst;

	public:
		Context() {
		}

		template<typename T = tea::Type, typename ...Args>
		T* getType(Args&&... args) {
			auto created = std::make_unique<T>(args...);
			for (const auto& ty : types) {
				if (ty->kind == created->kind && !memcmp(created.get(), ty.get(), sizeof(T)))
					return (T*)ty.get();
			}
			return (T*)types.emplace(std::move(created))->get();
		};
	};

	Context* getGlobalContext();
}
