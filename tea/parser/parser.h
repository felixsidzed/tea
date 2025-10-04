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

		typedef struct {
			int nargs;
		} FunctionPrototype;

		struct {
			std::vector<std::string> imported;
			std::unordered_map<std::string, FunctionPrototype> funcs;
			FunctionPrototype* curFunc = nullptr;
		} state;

		void parseBlock();
		void parseFunc(enum StorageType storage);
		std::unique_ptr<ExpressionNode> parseExpression();

		TEA_NORETURN unexpected();
	};
}
