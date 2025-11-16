#include <iostream>

#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "frontend/semantics/SemanticAnalyzer.h"

#include "mir/mir.h"
#include "mir/dump/dump.h"

int main() {
	try {
		const tea::vector<tea::frontend::Token>& tokens = tea::frontend::lex(R"(
public func main() -> int
	return 0;
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

		auto module = std::make_unique<tea::mir::Module>("[module]");
		tea::mir::Function* func = module->addFunction("main", tea::Type::Function(tea::Type::Int()));

		tea::mir::Builder builder;
		builder.insertInto(func->appendBlock("entry"));
		
		auto x = builder.alloca_(tea::Type::Int(), "");
		builder.store(x, tea::mir::ConstantNumber::get(67, 32));
		builder.ret(builder.load(x, ""));

		tea::mir::dump(module.get());
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
