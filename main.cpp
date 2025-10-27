#include <fstream>
#include <sstream>
#include <iostream>

#include "tea/tea.h"
#include "argparse.h"

int main(int argc, char** argv) {
	try {
		tea::args::Args* args = tea::args::parse(argc, argv);
		if (!args->source) {
			TEA_PANIC("source is a mandatory argument");
			TEA_UNREACHABLE();
		}

		if (args->force64Bit && args->force32Bit)
			TEA_PANIC("options '-64' and '-32' are mutually exclusive");

		if (args->force32Bit)
			args->force64Bit = false;
		else if (!args->force64Bit)
			args->force64Bit = sizeof(void*) == 8;

		if (!args->output) {
			std::string output;
			const std::string& source = args->source;

			size_t pos = source.rfind('.');
			if (pos != std::string::npos) output = source.substr(0, pos) + ".o";
			else output = source + ".o";

			args->output = new char[output.size() + 1];
			memcpy_s((char*)args->output, output.size() + 1, output.data(), output.size());
			((char*)args->output)[output.size() + 1] = '\0';
		}
		
		std::ifstream file(args->source);
		std::stringstream ss;
		ss << file.rdbuf();
		tea::compile({ ss.str().data(), (uint32_t)ss.str().size()}, args->output, args->importLookup, args->force64Bit, args->verbose, args->optimization);
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
