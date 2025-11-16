#pragma once

#include "frontend/lexer/Lexer.h"

#include "AST.h"
#include "common/tea.h"
#include "common/Type.h"

namespace tea::frontend {
	
	class Parser {
		const Token* cur;
		const Token* end;

		AST::Tree* tree = nullptr;
		tea::vector<AST::Tree*> treeHistory;

	public:
		Parser(const tea::vector<Token>& tokens)
			: cur(tokens.begin()), end(tokens.end()) {
		}

		AST::Tree parse();

	private:
		TEA_NORETURN unexpected();

		bool match(TokenKind kind);
		bool match(KeywordKind kind);

		const Token& consume(TokenKind kind);
		const Token& consume(KeywordKind kind);

		void parseBlock();
		std::unique_ptr<AST::Node> parseStat();

		tea::vector<std::pair<Type*, tea::string>> parseParams();

		tea::string parseType();

		std::unique_ptr<AST::ExpressionNode> parsePrimary();
		std::unique_ptr<AST::ExpressionNode> parseExpression();
		//std::unique_ptr<AST::ExpressionNode> parseRhs(int exprPrec, std::unique_ptr<AST::ExpressionNode> lhs);
	};

} // tea::frontend
