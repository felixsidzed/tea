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
		clock_t start = clock();

		const tea::vector<tea::frontend::Token>& tokens = tea::frontend::lex(R"(
public __cdecl func main() -> int
	var a = 2;
	return (a + a) * a;
end
)");

		tea::frontend::Parser parser(tokens);
		const tea::frontend::AST::Tree& ast = parser.parse();

		tea::frontend::analysis::SemanticAnalyzer analyzer;
		analyzer.importLookup.emplace(".");

		const auto& errors = analyzer.visit(ast);
		if (!errors.empty()) {
			fprintf(stderr, "SemanticAnalyzer: %d error%c:\n", errors.size, errors.size == 1 ? 0 : 's');
			for (const auto& error : errors)
				printf("  %.*s\n", (int)error.length(), error.data());
			return 1;
		}

		tea::CodeGen codegen;
		codegen.importLookup.emplace(".");
		auto module = codegen.emit(ast);

		tea::backend::MIRLowering lowering;
		auto [mc, size] = lowering.lower(module.get());

		std::ofstream stream("x64/Debug/test.o");
		stream.write((char*)mc.get(), size);
		stream.close();
		
		printf("Wrote to 'x64/Debug/test.o'. Compilation took %ldms\n", clock() - start);

	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
