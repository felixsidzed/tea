#include "Parser.h"

#include <unordered_map>

#define next() (++cur)

#define mknode(T, ...) std::make_unique<T>(__VA_ARGS__, _line, _column)

#define pushtree(T, ...) { \
	auto node = std::make_unique<T>(__VA_ARGS__, _line, _column); \
	auto body = &node->body; \
	treeHistory.push(tree); \
	tree->push(std::move(node)); \
	tree = body; \
}

#define poptree() { \
	tree = treeHistory[treeHistory.size-1]; \
	treeHistory.pop(); \
}

namespace tea::frontend {

	AST::Tree Parser::parse() {
		treeHistory.clear();

		AST::Tree root;
		tree = &root;
		treeHistory.push(tree);

		while (!match(TokenKind::EndOfFile)) {
			uint32_t _line = cur->line, _column = cur->column;

			if (cur->kind != TokenKind::Keyword)
				unexpected();

			switch ((KeywordKind)cur->extra) {

			case KeywordKind::Public:
			case KeywordKind::Private: {
				AST::StorageClass vis = (AST::StorageClass)cur->extra;
				next();
				
				if ((KeywordKind)cur->extra == KeywordKind::Func) {
					next();
					const tea::string& name = consume(TokenKind::Identf).text;
					const auto& params = parseParams();

					consume(TokenKind::Arrow);

					const tea::string& typeName = parseType();
					Type* returnType = Type::get(typeName);
					if (!returnType)
						TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

					pushtree(AST::FunctionNode, vis, name, params, returnType);
					parseBlock();
					break;
				}
				TEA_FALLTHROUGH;
			}

			default:
				unexpected();
				break;
			}
		}

		return root;
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", cur->text.data(), cur->line, cur->column);
	}

	bool Parser::match(TokenKind kind) {
		if (cur->kind == kind) {
			next();
			return true;
		}
		return false;
	};
	bool Parser::match(KeywordKind kind) {
		if (cur->kind == TokenKind::Keyword && cur->extra == (uint32_t)kind) {
			next();
			return true;
		}
		return false;
	};

	const Token& Parser::consume(TokenKind kind) {
		if (cur->kind != kind)
			unexpected();
		return *cur++;
	};
	const Token& Parser::consume(KeywordKind kind) {
		if (cur->kind != TokenKind::Keyword || cur->extra != (uint32_t)kind)
			unexpected();
		return *cur++;
	};

	void Parser::parseBlock() {
		while (true) {
			if (cur->kind == TokenKind::Keyword && cur->extra == (uint32_t)KeywordKind::End) {
				next();
				poptree();
				return;
			}

			uint32_t _line = cur->line, _column = cur->column;
			auto node = parseStat();
			node->line = _line; node->column = _column;
			tree->push(std::move(node));

			if (match(TokenKind::EndOfFile))
				TEA_PANIC("unexpected EOF (did you forget to close a function?). line %d, column %d", cur->line, cur->column);
		}
	};
	std::unique_ptr<AST::Node> Parser::parseStat() {
		uint32_t _line = cur->line, _column = cur->column;

		switch (cur->kind) {
		case TokenKind::Keyword: {
			switch ((KeywordKind)cur->extra) {
			case KeywordKind::Return: {
				next();
				auto node = mknode(AST::ReturnNode, parseExpression());
				consume(TokenKind::Semicolon);
				return std::move(node);
			} break;

			default:
				unexpected();
				break;
			}
		} break;

		default:
			unexpected();
			break;
		}
		return nullptr;
	};

	tea::vector<std::pair<Type*, tea::string>> Parser::parseParams() {
		// TODO
		consume(TokenKind::Lpar);
		consume(TokenKind::Rpar);
		return {};
	};

	tea::string Parser::parseType() {
		tea::string typeName;
		tea::string first;
		if (cur->kind == TokenKind::Identf)
			first = cur->text;
		else
			unexpected();
		next();

		if (first == "const") {
			typeName = "const ";
			tea::string next = consume(TokenKind::Identf).text;
			if (next == "signed" || next == "unsigned") {
				typeName += next;
				typeName += ' ';
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += next;
		} else if (first == "signed" || first == "unsigned") {
			typeName = first;
			typeName += ' ';
			tea::string next = consume(TokenKind::Identf).text;
			if (next == "const") {
				typeName += "const ";
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += next;
		} else
			typeName += first;

		return typeName;
	};

	std::unique_ptr<AST::ExpressionNode> Parser::parsePrimary() {
		uint32_t _line = 0, _column = 0;
		std::unique_ptr<AST::ExpressionNode> node = nullptr;

		switch (cur->kind) {
		case TokenKind::Int: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Int, cur->text);
			next();
			break;
		}

		case TokenKind::Float: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Int, cur->text);
			next();
			break;
		}

		case TokenKind::Double: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Int, cur->text);
			next();
			break;
		}
		case TokenKind::String: {
			node = mknode(AST::LiteralNode, AST::ExprKind::String, cur->text);
			next();
			break;
		}
		case TokenKind::Char: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Char, cur->text);
			next();
			break;
		}
		case TokenKind::Identf: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Identf, cur->text);
			next();
			break;
		}
		case TokenKind::Lpar: {
			next();
			node = parseExpression();
			consume(TokenKind::Rpar);
			break;
		}
		default:
			unexpected();
		}

		return node;
	}

	/*static int getPrecedence(TokenKind kind) {
		switch (kind) {

		case TokenKind::Or:
			return 1;

		case TokenKind::And:
			return 2;

		case TokenKind::Bor:
			return 3;

		case TokenKind::Bxor:
			return 4;

		case TokenKind::Amp:
			return 5;

		case TokenKind::Eq:
		case TokenKind::Neq:
			return 6;

		case TokenKind::Lt:
		case TokenKind::Le:
		case TokenKind::Gt:
		case TokenKind::Ge:
			return 7;

		case TokenKind::Shl:
		case TokenKind::Shr:
			return 8;

		case TokenKind::Add:
		case TokenKind::Sub:
			return 9;

		case TokenKind::Star:
		case TokenKind::Div:
			return 10;

		default:
			return -1;
		}
	}

	static const std::unordered_map<TokenKind, AST::ExprKind> tt2et = {
		{TokenKind::String, AST::ExprKind::String},
		{TokenKind::Char, AST::ExprKind::Char},
		{TokenKind::Int, AST::ExprKind::Int},
		{TokenKind::Float, AST::ExprKind::Float},
		{TokenKind::Double, AST::ExprKind::Double},
		{TokenKind::Identf, AST::ExprKind::Identf}
	};

	std::unique_ptr<AST::ExpressionNode> Parser::parseRhs(int lhsPrec, std::unique_ptr<AST::ExpressionNode> lhs) {
		while (true) {
			int prec = getPrecedence(cur->kind);
			if (prec < lhsPrec)
				return lhs;
			TokenKind tt = cur->kind;
			next();
			auto rhs = parsePrimary();
			int nextPrec = getPrecedence(cur->kind);
			if (prec < nextPrec)
				rhs = parseRhs(prec + 1, std::move(rhs));
			lhs = std::make_unique<AST::BinaryExpr>(*tt2et.find(tt), std::move(lhs), std::move(rhs), (cur - 1)->line, lhs->column = (cur - 1)->column);
		}
	}

	std::unique_ptr<AST::ExpressionNode> Parser::parseExpression() {
		auto lhs = parsePrimary();
		return parseRhs(0, std::move(lhs));
	}*/
	std::unique_ptr<AST::ExpressionNode> Parser::parseExpression() {
		return parsePrimary();
	}

} // tea::frontend
