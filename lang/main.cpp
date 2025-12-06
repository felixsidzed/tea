#include <fstream>
#include <iostream>

#include "argparse.h"
#include "common/tea.h"

int main(int argc, char** argv) {
	try {
		tea::args::Args* args = tea::args::parse(argc, argv);
		if (!args->source) {
			TEA_PANIC("source is a mandatory argument");
			TEA_UNREACHABLE();
		}

		if (!args->outFile) {
			std::string output;
			const std::string& source = args->source;

			size_t pos = source.rfind('.');
			if (pos != std::string::npos) output = source.substr(0, pos) + ".o";
			else output = source + ".o";

			args->outFile = new char[output.size() + 1];
			memcpy_s(args->outFile, output.size() + 1, output.data(), output.size());
			args->outFile[output.size() + 1] = '\0';
		}

		std::ifstream file(args->source);
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		tea::compile({ content.data(), (uint32_t)content.size() }, args->importLookup, args->outFile, args->triple, args->flags, args->optLevel);

	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
