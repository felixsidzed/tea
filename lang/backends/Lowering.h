#pragma once

#include "mir/mir.h"

namespace tea::backend {

	class Lowering {
	public:
		tea::Context& ctx;

		struct Options {
			const char* outfile = nullptr;
			bool dumpModule : 1 = true;
			uint8_t optLevel : 2 = 0;
		};

		Options options;

		Lowering(tea::Context& ctx) : ctx(ctx) {};

		virtual tea::string name() = 0;
		virtual tea::vector<tea::string> supportedTargets() = 0;

		virtual void lower(const mir::Module* module, Options options = {}) = 0;
	};

} // namespace tea::backend
