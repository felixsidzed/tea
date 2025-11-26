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

		tea::mir::Builder builder;
		auto module = std::make_unique<tea::mir::Module>("[module]");

		tea::Type* i32 = tea::Type::Int();
		auto ty = tea::Type::Struct(&i32, 1, "MyStruct");

		tea::mir::Function* main = module->addFunction("main", tea::Type::Function(i32));
		builder.insertInto(main->appendBlock("entry"));

		auto pfield = builder.gep(tea::mir::ConstantPointer::get(ty, 0x676767), tea::mir::ConstantNumber::get(0, 32), "");
		builder.ret(builder.load(pfield, "", true));

		tea::mir::dump(module.get());
		
#ifdef _DEBUG
		printf("Compilation took %ldms\n", clock() - start);
#endif
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return 1;
	}

	return 0;
}
