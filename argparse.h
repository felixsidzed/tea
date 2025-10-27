#pragma once

#include <cstdint>

#include "tea/vector.h"

namespace tea {
	// TODO: maybe we should prioritize speed over size
	namespace args {
#pragma warning(push)
#pragma warning(disable : 4200) // nonstandard extension used: zero-sized array in struct/union
		struct Args {
			const char* output = nullptr;
			bool verbose = false;
			bool force32Bit = false;
			bool force64Bit = true;
			uint8_t optimization = 2;
			vector<const char*> importLookup;

			const char* source = nullptr;
		};
#pragma warning(pop)

		Args* parse(int argc, char** argv);
	}
}
