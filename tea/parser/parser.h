#pragma once

#include <vector>
#include <unordered_map>

#include "tea/tea.h"
#include "tea/vector.h"
#include "tea/parser/node.h"
#include "tea/lexer/token.h"

namespace tea {
	class Parser {
	public:
		Parser();

		Tree parse(const vector<Token>& tokens);
	private:
		Tree* tree;
		vector<Tree*> treeHistory;

		const Token* t;

		vector<string> funcs;
		vector<string> imported;

		FunctionNode* fn;

		void parseFuncFull();
		bool tryParseAssignment();
		void parseFunc(enum StorageType storage);
		string parseType(bool ignoreNl = true);
		std::unique_ptr<ExpressionNode> parsePrimary();
		std::unique_ptr<ExpressionNode> parseExpression();
		void parseBlock(const vector<enum KeywordType>& extraTerminators = {});
		std::unique_ptr<ExpressionNode> parseRhs(int exprPrec, std::unique_ptr<ExpressionNode> lhs);

		TEA_NORETURN unexpected();
	};
}
