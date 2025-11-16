#pragma once

#include <string>

#include "common/map.h"
#include "common/string.h"
#include "common/vector.h"

namespace tea::mir {
	class Scope {
	public:
		Scope() {
			used.emplace("");
		}

		bool isUsed(const tea::string& name) const {
			return used.find(name) != nullptr;
		}

		const char* add(const tea::string& name, bool dedup = true) {
			if (dedup) {
				if (!isUsed(name))
					return used.emplace(name)->data();

				tea::string base = name;
				tea::string candidate = base;

				int counter = 1;
				do
					candidate = base + "." + std::to_string(counter++).c_str();
				while (isUsed(candidate));

				return used.emplace(candidate)->data();
			}
			else {
				if (isUsed(name))
					return nullptr;
				return used.emplace(name)->data();
			}
		}

	private:
		tea::vector<tea::string> used;
	};
}
