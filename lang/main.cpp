#include <fstream>
#include <iostream>

#include "common/tea.h"
#include "common/Type.h"
#include "mir/Context.h"

int main() {
	try {
		std::ifstream file("playground/test.tea");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		tea::compile(content.c_str(), {"../teastd"}, "../x64/Debug/test.o", nullptr, true, 0);

	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
