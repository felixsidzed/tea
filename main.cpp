#include <fstream>
#include <sstream>
#include <iostream>

#include "tea/tea.h"

int main() {
	std::ifstream file("playground/test.tea");
	std::stringstream buffer;
	buffer << file.rdbuf();

	const std::string& src = buffer.str();
	try {
		tea::compile(src, "x64/Debug/build/test.o", "stdlib", true, true);
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
