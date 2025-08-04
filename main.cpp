#include <fstream>
#include <iostream>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"

int main() {
	auto src = R"(
private func compute(int a, int b) -> int
	return a + b;
end

public func main() -> int
	return compute(3, 5);
end
)";

	tea::Lexer lexer;
	const auto& tokens = lexer.lex(src);

	tea::Parser parser;
	const auto& root = parser.parse(tokens);

	tea::CodeGen codegen(true, true);
	codegen.emit(root, "x64/Debug/build/test.o");

	return 0;
}
