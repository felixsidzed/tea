#pragma once

#include "AST.h"
#include "core/tea.h"
#include "core/Type.h"
#include "core/context.h"
#include "frontend/lexer/Lexer.h"

namespace tea::frontend {
	class Parser {
		tea::Context& ctx;

		uint32_t fsrc = 0;
		const Token* cur = nullptr;
		const Token* end = nullptr;

		AST::Tree* tree = nullptr;
		AST::FunctionNode* func = nullptr;
		tea::vector<AST::Tree*> treeHistory;
		tea::vector<std::unique_ptr<AST::AttributeNode>> attrStack;

	public:
		Parser(tea::Context& ctx) : ctx(ctx) {}

		AST::Tree parse(const tea::vector<Token>& tokens, uint32_t fsrc);

	private:
		void unexpected();

		bool match(TokenKind kind);
		bool match(KeywordKind kind);

		const Token& consume(TokenKind kind);
		const Token& consume(KeywordKind kind);

		std::unique_ptr<AST::Node> parseStat();
		void parseBlock(const tea::vector<KeywordKind>& extraTerminators = {});
		std::unique_ptr<AST::VariableNode> parseVariable(uint32_t _line, uint32_t _column);

		void parseFuncImport(uint32_t _line, uint32_t _column);
		void parseFunc(AST::StorageClass vis, uint32_t _line, uint32_t _column);
		std::unique_ptr<AST::GlobalVariableNode> parseGlobalVariable(AST::StorageClass vis, uint32_t _line, uint32_t _column);

		std::pair<bool, tea::vector<std::pair<Type*, tea::string>>> parseParams();

		tea::string parseType();

		std::unique_ptr<AST::ExpressionNode> parsePrimary(bool allowAssignments = true);
		std::unique_ptr<AST::ExpressionNode> parseExpression(bool allowAssignments = true);
		std::unique_ptr<AST::ExpressionNode> parseRhs(int exprPrec, std::unique_ptr<AST::ExpressionNode> lhs);
	};

} // tea::frontend
