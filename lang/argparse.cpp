#include "argparse.h"

#include "common/map.h"

namespace tea::args {

	tea::map<tea::string, CompilerFlags::Flag> name2flag = {
		{"dump-mir", CompilerFlags::DumpMIR},
		{"dump-final-ir", CompilerFlags::DumpFinalIR}
	};

	static void help(char* exeName) {
		printf(
			"usage: \"%s\" <source> [...options]\n\n"
			"options:\n"
			"  -o, --output <file>     output file\n"
			"  -t, --triple <triple>   set target triple\n"
			"  -O[0-3]                 optimization level\n"
			"  -I <path>               add import search path\n"
			"  -h, --help              show this message and quit\n"
			"  -f, --flag <name>       enable a compiler flag\n\n"
			"flags:\n",
			exeName
		);
		for (const auto& [name, _] : name2flag)
			printf("  %s\n", name.data());
	}

	Args* parse(int argc, char** argv) {
		if (argc < 2) {
			help(*argv);
			exit(1);
		}

		Args* args = (Args*)calloc(1, sizeof(Args));
		if (!args) {
			TEA_PANIC("bad alloc");
			TEA_UNREACHABLE();
		}

		for (int i = 1; i < argc; i++) {
			const char* arg = argv[i];

			if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				help(*argv);
				exit(0);
			}

			if (*arg == '-') {
				if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
					if (++i >= argc)
						TEA_PANIC("missing output file after '%s'", arg);
					args->outFile = argv[i];

				} else if (!strcmp(arg, "-t") || !strcmp(arg, "--triple")) {
					if (++i >= argc)
						TEA_PANIC("missing triple after '%s'", arg);
					args->triple = argv[i];

				} else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose"))
					args->verbose = true;

				else if (arg[1] == 'O' && arg[2] >= '0' && arg[2] <= '3')
					args->optLevel = arg[2] - '0';

				else if (!strcmp(arg, "-I")) {
					if (++i >= argc)
						TEA_PANIC("missing include path after '%s'", arg);
					args->importLookup.emplace(argv[i]);

				} else if (!strcmp(arg, "-f") || !strcmp(arg, "--flag")) {
					if (++i >= argc) TEA_PANIC("missing flag after '%s'", arg);

					if (auto* it = name2flag.find(argv[i]))
						args->flags |= *it;
					else
						TEA_PANIC("invalid flag: '%s'");

				} else
					TEA_PANIC("invalid argument at %d: '%s'", i, arg);
			} else {
				if (args->source)
					TEA_PANIC("multiple source files specified: '%s'", arg);
				args->source = arg;
			}
		}

		if (!args->source)
			TEA_PANIC("no input source provided");

		return args;
	}

} // namespace tea
