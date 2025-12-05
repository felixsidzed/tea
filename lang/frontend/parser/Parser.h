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
		AST::FunctionNode* func = nullptr;
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

		std::unique_ptr<AST::Node> parseStat();
		void parseBlock(const tea::vector<KeywordKind>& extraTerminators = {});
		std::unique_ptr<AST::VariableNode> parseVariable(uint32_t _line, uint32_t _column);

		void parseFuncImport(uint32_t _line, uint32_t _column);
		void parseFunc(AST::StorageClass vis, uint32_t _line, uint32_t _column);

		std::pair<bool, tea::vector<std::pair<Type*, tea::string>>> parseParams();

		tea::string parseType();

		std::unique_ptr<AST::ExpressionNode> parsePrimary(bool allowAssignments = true);
		std::unique_ptr<AST::ExpressionNode> parseExpression(bool allowAssignments = true);
		std::unique_ptr<AST::ExpressionNode> parseRhs(int exprPrec, std::unique_ptr<AST::ExpressionNode> lhs);
	};

} // tea::frontend
