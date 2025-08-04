#pragma once

#include <vector>
#include <unordered_map>

#include "tea.h"
#include "node.h"
#include "lexer/token.h"

namespace tea {
	class Parser {
	public:
		Parser();

		Tree parse(const std::vector<Token>& tokens);
	private:
		Tree* tree;
		std::vector<Tree*> treeHistory;

		const Token* t;

		std::vector<std::string> funcs;
		std::vector<std::string> imported;

		void parseBlock();
		void parseFunc(enum StorageType storage);
		std::unique_ptr<ExpressionNode> parseExpression();
		std::unique_ptr<ExpressionNode> parsePrimary();
		std::unique_ptr<ExpressionNode> parseRhs(int exprPrec, std::unique_ptr<ExpressionNode> lhs);

		TEA_NORETURN unexpected();
	};
}
