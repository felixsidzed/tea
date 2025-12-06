#pragma once

#include <cstdint>

#include "common/tea.h"

namespace tea::args {
	// TODO: maybe we should prioritize speed over size
	struct Args {
		const char* source = nullptr;
		tea::vector<const char*> importLookup;
		char* outFile = nullptr;
		const char* triple = nullptr;
		bool verbose = false;
		uint8_t optLevel = 2;
		CompilerFlags flags = CompilerFlags::None;
	};

	Args* parse(int argc, char** argv);
}