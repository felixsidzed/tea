#include <fstream>
#include <iostream>
#include <filesystem>

#include "argparse.h"
#include "core/tea.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
	tea::Context ctx;
	try {
		tea::args::Args args = tea::args::parse(ctx, argc, argv);

		for (const char* p : args.importLookup)
			ctx.importLookup.emplace(p);

		for (const char* input : args.inputs) {
			tea::string outFile;

			if (!args.outFile.empty() && args.inputs.size == 1)
				outFile = args.outFile;
			else {
				fs::path path = std::string(input);
				outFile = path.replace_extension(".o").string().c_str();
			}

			uint32_t fsrc = ctx.sources.load(input);
			if (fsrc == tea::badid)
				ctx.diag.error(TEA_NO_SOURCELOC, 2, "could not open file: %s", input);

			tea::compile(ctx, fsrc, outFile, args.triple, args.flags, args.optLevel);
		}

	} catch (const std::exception&) {
		ctx.diag.print();
		return 1;
	}

	ctx.diag.print();
	return (int)ctx.diag.hasError;
}
