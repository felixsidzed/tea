#include "tea.h"

#include <fstream>

#include "mir/dump/dump.h"
#include "codegen/codegen.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"
#include "backends/llvm/LLVMLowering.h"
#include "backends/luau/LuauLowering.h"
#include "frontend/analysis/Analyzer.h"

namespace tea {
	void compile(
		Context& ctx, uint32_t fsrc,
		const char* outfile, const char* fqTarget,
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

		tea::frontend::Analyzer analyzer(ctx);
		analyzer.visit(ast, fsrc);
		if (ctx.diag.hasError)
			return;

		tea::string backendName = "llvm";
		tea::string target = fqTarget ? fqTarget : "";

		if (fqTarget) {
			const char* at = strchr(fqTarget, '@');
			if (at) {
				backendName = tea::string(fqTarget, at - fqTarget);
				target = at + 1;
			}
		}

		if (target.empty()) {
			ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "missing target");
			return;
		}

		tea::CodeGen codegen(ctx);
		tea::CodeGen::Options coptions;
		if (fqTarget)
			coptions.target = target;

		auto module = codegen.emit(fsrc, ast, coptions);
		if (flags.has(CompilerFlags::DumpMIR)) {
			tea::mir::dump(module.get());
			putchar('\n');
		}

		if (ctx.diag.hasError)
			return;

		std::unique_ptr<tea::backend::Lowering> lowering = nullptr;
		if (backendName == "llvm") {
			lowering = std::make_unique<tea::backend::LLVMLowering>(ctx);
		} else if (backendName == "luau") {
			lowering = std::make_unique<tea::backend::LuauLowering>(ctx);
		} else {
			ctx.diag.fatal(TEA_NO_SOURCELOC, 0, "unknown backend '%s'", backendName.data());
			return;
		}

		auto it = lowering->supportedTargets().find(target);
		if (!it) {
			ctx.diag.fatal(TEA_NO_SOURCELOC, 1, "the backend '%s' doesn't support the target '%s'", backendName.data(), target.data());
			return;
		}

		lowering->lower(module.get(), {
			.outfile = outfile,
			.dumpModule = flags.has(CompilerFlags::DumpFinalIR),
			.optLevel = optLevel,
		});

		double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		printf("Compilation took %ldm %lds %ldms\n",
			(long)(diff / 60),
			(long)diff % 60,
			(long)((diff - (long)diff) * 1000));
	}
}
