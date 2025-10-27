#include "argparse.h"

#include <cstring>

#include "tea/tea.h"

namespace tea {
	args::Args* args::parse(int argc, char** argv) {
		Args* args = (Args*)malloc(sizeof(Args));
		if (!args) {
			TEA_PANIC("bad alloc");
			TEA_UNREACHABLE();
		}
		memset(args, 0, sizeof(Args));

		for (int i = 1; i < argc; i++) {
			const char* arg = argv[i];
			if (*arg == '-') {
				switch (*(arg + 1)) {
				case 'o':
				output:
					args->output = argv[++i];
					break;

				case 'v':
				verbose:
					args->verbose = true;
					break;

				case '6':
					if (*(arg + 2) == '4') {
					force64Bit:
						args->force64Bit = true;
						break;
					}
					goto invalid;

				case '3':
					if (*(arg + 2) == '2') {
					force32Bit:
						args->force32Bit = true;
						break;
					}
					goto invalid;

				case 'O':
					if (*(arg + 2) >= '0' && *(arg + 2) <= '3') {
						args->optimization = '0' - *(arg + 2);
						break;
					}
					goto invalid;

				case 'I': {
					const char* ch = arg;
					string path;
					while (*++ch != ' ')
						path += *ch;
					if (path.empty())
						goto invalid;
					args->importLookup.push(argv[i] + 2);
				} break;

				default:
					if (!strcmp(arg, "--output")) goto output;
					else if (!strcmp(arg, "--verbose")) goto verbose;
					else if (!strcmp(arg, "--force64Bit")) goto force64Bit;
					else if (!strcmp(arg, "--force32Bit")) goto force32Bit;
					else goto invalid;
					break;
				}
			} else {
				if (args->source)
				invalid:
					TEA_PANIC("invalid argument at %d: %s", i, arg);
				else
					args->source = arg;
			}
		}

		return args;
	}
}
