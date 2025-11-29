#include <fstream>
#include <iostream>

#include "mir/dump/dump.h"
#include "codegen/codegen.h"
#include "backend/lowering.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "frontend/semantics/SemanticAnalyzer.h"

int main() {
	try {
		tea::compile(R"(
public func main() -> int
	var a = 2;
	return (a + a) * a;
end
)", { "." }, "x64/Debug/test.o", nullptr, true, 0);

	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
