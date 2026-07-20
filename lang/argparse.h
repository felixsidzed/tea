#pragma once

#include <cstdint>
#include <optional>

#include "core/tea.h"
#include "core/context.h"

namespace tea::args {
	struct Args {
		tea::string outFile;

		uint8_t optLevel = 2;
		bool verbose = false;
		bool compileOnly = true;
		CompilerFlags flags = CompilerFlags::None;

		const char* triple = nullptr;

		tea::vector<const char*> inputs;
		tea::vector<const char*> importLookup;
	};

	Args parse(Context& ctx, int argc, char** argv);
}
