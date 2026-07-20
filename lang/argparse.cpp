#include "argparse.h"

#include "core/map.h"

namespace tea::args {

	static const tea::map<tea::string, CompilerFlags::Flag> name2flag = {
		{"dump-mir", CompilerFlags::DumpMIR},
		{"dump-final-ir", CompilerFlags::DumpFinalIR}
	};

	static void help(const char* exeName) {
		printf(
			"usage: \"%s\" [options] <files...>\n\n"
			"options:\n"
			"  -o <file>               write output to <file>\n"
			//"  -c                      compile only (no linking)\n" // TODO: finish bond and implement me
			"  -t, --triple <triple>   set target triple\n"
			"  -O[0-3]                 optimization level\n"
			"  -I <dir>                add import search path\n"
			//"  -l <name>               link with library\n"
			"  -v, --verbose           verbose output\n"
			"  -h, --help              show this message\n"
			"  -f, --flag <name>       enable compiler flag\n\n"
			"flags:\n", exeName
		);
		for (const auto& [name, _] : name2flag)
			printf("  %s\n", name.data());
	}

	Args parse(Context& ctx, int argc, char** argv) {
		if (argc < 2) {
			help(argv[0]);
			exit(1);
		}

		Args args;

		for (int i = 1; i < argc; i++) {
			const char* arg = argv[i];

			if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
				help(argv[0]);
				exit(0);
			}

			if (*arg == '-') {
				if (!strcmp(arg, "-o")) {
					if (++i >= argc)
						ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "missing output file after '%s'", arg);
					args.outFile = argv[i];
				}
				//else if (!strcmp(arg, "-c")) // TODO
				//	args.compileOnly = true;

				else if (!strcmp(arg, "-t") || !strcmp(arg, "--triple")) {
					if (++i >= argc)
						ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "missing triple after '%s'", arg);
					args.triple = argv[i];
				}
				else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose"))
					args.verbose = true;

				else if (arg[1] == 'O' && arg[2] >= '0' && arg[2] <= '3' && arg[3] == '\0')
					args.optLevel = (uint8_t)(arg[2] - '0');

				else if (!strcmp(arg, "-I")) {
					if (++i >= argc)
						ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "missing import path after '%s'", arg);
					args.importLookup.emplace(argv[i]);
				}
				else if (!strcmp(arg, "-f") || !strcmp(arg, "--flag")) {
					if (++i >= argc)
						ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "missing flag after '%s'", arg);

					if (auto* it = name2flag.find(argv[i]))
						args.flags |= *it;
					else
						ctx.diag.error(TEA_NO_SOURCELOC, 1, "invalid flag: '%s'", argv[i]);

				} else
					ctx.diag.error(TEA_NO_SOURCELOC, 1, "invalid argument at %d: '%s'", i, arg);

			} else
				args.inputs.emplace(argv[i]);
		}

		if (args.inputs.empty())
			ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "no input files");

		return args;
	}

} // namespace tea
