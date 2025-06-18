#pragma once

#include <vector>

#include "tree.h"
#include "lexer/token.h"

namespace tea {
	class Parser {
	public:
		Parser();

		Tree parse(const std::vector<Token>& tokens);
	private:
		Tree* tree;
		const Token* t;
	};
}
