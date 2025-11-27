#pragma once

#include "mir/mir.h"

#include "frontend/parser/AST.h"

namespace tea {

	using namespace frontend;

	class CodeGen {
		mir::Builder builder;
		std::unique_ptr<mir::Module> module = nullptr;
		tea::vector<std::pair<Type*, tea::string>>* curParams = nullptr;

	public:
		struct Options {
			tea::string triple;
			mir::DataLayout dl;
		};

		std::unique_ptr<mir::Module> emit(const AST::Tree& tree, const Options& options = {});

	private:
		void emitBlock(const AST::Tree& tree);
		mir::Value* emitExpression(const AST::ExpressionNode* expr);
	};

} // namespace tea
