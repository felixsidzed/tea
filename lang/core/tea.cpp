#include "tea.h"

#include <fstream>

#include "mir/dump/dump.h"
#include "codegen/codegen.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "backends/llvm/LLVMLowering.h"
#include "backends/luau/LuauLowering.h"
#include "frontend/semantics/SemanticAnalyzer.h"

namespace tea {
	void compile(
		Context& ctx, uint32_t fsrc,
		const char* outfile, const char* triple,
		const CompilerFlags& flags, uint8_t optLevel
	) {
		clock_t start = clock();

		const tea::vector<tea::frontend::Token>& tokens = tea::frontend::lex(ctx, fsrc);
		if (ctx.diag.hasError)
			return;

		tea::frontend::Parser parser(ctx);
		const tea::frontend::AST::Tree& ast = parser.parse(tokens, fsrc);
		if (ctx.diag.hasError)
			return;

		tea::frontend::SemanticAnalyzer analyzer(ctx);
		analyzer.visit(ast, fsrc);
		if (ctx.diag.hasError)
			return;

		tea::CodeGen codegen(ctx);
		tea::CodeGen::Options coptions;
		if (triple)
			coptions.triple = triple;
		auto module = codegen.emit(fsrc, ast, coptions);

		if (flags.has(CompilerFlags::DumpMIR)) {
			tea::mir::dump(module.get());
			putchar('\n');
		}

		if (ctx.diag.hasError)
			return;

		if (module->triple == "experimental-luau-0.730") {
			tea::backend::LuauLowering lowering(ctx);
			lowering.lower(module.get(), {
				.outfile = outfile,
				.dumpModule = flags.has(CompilerFlags::DumpFinalIR),
				.optLevel = optLevel
			});
		} else {
			tea::backend::LLVMLowering lowering(ctx);
			lowering.lower(module.get(), {
				.outfile = outfile,
				.dumpModule = flags.has(CompilerFlags::DumpFinalIR),
				.optLevel = optLevel,
			});
		}

		double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		printf("Compilation took %ldm %lds %ldms\n",
			(long)(diff / 60),
			(long)diff % 60,
			(long)((diff - (long)diff) * 1000));
	}
}
