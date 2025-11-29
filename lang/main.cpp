#include <fstream>
#include <iostream>

#include "common/tea.h"

int main() {
	try {
		tea::compile(R"(
public func main() -> int
	var i = 0;
	if (i == 0) do
		return 123;
	else
		return 67;
	end
end
)", { "." }, "x64/Debug/test.o", nullptr, true, 0);

	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
