#pragma once

#include <memory>

#include "common/map.h"
#include "common/Type.h"

#include "mir/mir.h"

namespace tea::mir {
	class Context {
		friend struct Type;

		friend class ConstantArray;
		friend class ConstantNumber;
		friend class ConstantString;
		friend class ConstantPointer;

		// TODO ASAP: migrate to hash map
		tea::vector<std::unique_ptr<tea::Type>> types;
		tea::map<size_t, std::unique_ptr<tea::StructType>> structTypes;
		tea::map<size_t, std::unique_ptr<tea::FunctionType>> funcTypes;

		tea::map<size_t, std::unique_ptr<ConstantArray>> arrConst;
		tea::map<size_t, std::unique_ptr<ConstantString>> strConst;

		tea::map<uint8_t, std::unique_ptr<ConstantNumber>> num0Const;
		tea::map<uint8_t, std::unique_ptr<ConstantNumber>> num1Const;
		tea::map<uint64_t, std::unique_ptr<ConstantNumber>> numConst;

		tea::map<size_t, std::unique_ptr<ConstantPointer>> ptrConst;

	public:
		Context() {
		}

		// TODO ASAP: migrate to hash map
		template<typename T = tea::Type, typename ...Args>
		T* getType(Args&&... args) {
			auto created = std::make_unique<T>(args...);
			for (const auto& ty : types) {
				if (ty->kind == created->kind && !memcmp(created.get(), ty.get(), sizeof(T)))
					return (T*)ty.get();
			}
			return (T*)types.emplace(std::move(created))->get();
		};

		template<>
		tea::StructType* getType<tea::StructType>() = delete;

		template<>
		tea::FunctionType* getType<tea::FunctionType>() = delete;
	};

	Context* getGlobalContext();
}
