#include <iostream>

#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "frontend/semantics/SemanticAnalyzer.h"

#include "mir/mir.h"
#include "mir/dump/dump.h"

int main() {
	try {
#ifdef _DEBUG
		clock_t start = clock();
#endif

		const tea::vector<tea::frontend::Token>& tokens = tea::frontend::lex(R"(
public func foo() -> int
	return 67;
end

public func main() -> int
	return foo();
end
)");

		tea::frontend::Parser parser(tokens);
		const tea::frontend::AST::Tree& ast = parser.parse();

		tea::frontend::analysis::SemanticAnalyzer analyzer;
		const auto& errors = analyzer.visit(ast);
		if (!errors.empty()) {
			fprintf(stderr, "SemanticAnalyzer: %d error%c:\n", errors.size, errors.size == 1 ? 0 : 's');
			for (const auto& error : errors)
				printf("  %.*s\n", (int)error.length(), error.data());
			return 1;
		}

		printf("%d\n", ast.size);
		
#ifdef _DEBUG
		printf("Compilation took %ldms\n", clock() - start);
#endif
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
