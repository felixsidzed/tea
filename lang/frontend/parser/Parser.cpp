#include "Parser.h"

#include <unordered_map>

#define next() (++cur)

#define mknodei(T, i, ...) std::make_unique<T>(__VA_ARGS__, _line##i, _column##i)
#define mknode(T, ...) std::make_unique<T>(__VA_ARGS__, _line, _column)

#define pushtree(T, ...) { \
	auto node = std::make_unique<T>(__VA_ARGS__, _line, _column); \
	auto body = &node->body; \
    treeHistory.emplace(tree); \
	tree->emplace(std::move(node)); \
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
		treeHistory.emplace(tree);

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
				unexpected();
			}

			case KeywordKind::Import: {
				next();
				
				if (match(KeywordKind::Func)) {
					const tea::string& name = consume(TokenKind::Identf).text;
					const auto& params = parseParams();

					consume(TokenKind::Arrow);

					const tea::string& typeName = parseType();
					Type* returnType = Type::get(typeName);
					if (!returnType)
						TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

					auto node = mknode(AST::FunctionImportNode, name, params, returnType);
					consume(TokenKind::Semicolon);
					tree->emplace(std::move(node));
				} else
					unexpected();
			} break;

			case KeywordKind::Using: {
				next();

				const tea::string& path = consume(TokenKind::String).text;
				auto node = mknode(AST::ModuleImportNode, path);
				consume(TokenKind::Semicolon);
				tree->emplace(std::move(node));
			} break;

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
			tree->emplace(std::move(node));

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

		case TokenKind::Identf: {
			auto node = parseExpression();
			consume(TokenKind::Semicolon);
			return std::move(node);
		}

		default:
			unexpected();
			break;
		}
		return nullptr;
	};

	tea::vector<std::pair<Type*, tea::string>> Parser::parseParams() {
		tea::vector<std::pair<Type*, tea::string>> result;
		consume(TokenKind::Lpar);

		if (match(TokenKind::Rpar))
			return result;

		while (true) {
			const tea::string& typeName = parseType();
			Type* type = Type::get(typeName);
			if (!type)
				TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

			result.emplace(
				type,
				consume(TokenKind::Identf).text
			);

			if (match(TokenKind::Rpar))
				break;

			consume(TokenKind::Comma);
		}

		return result;
	}

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
			tea::string nextToken = consume(TokenKind::Identf).text;
			if (nextToken == "signed" || nextToken == "unsigned") {
				typeName += nextToken + ' ';
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += nextToken;
		} else if (first == "signed" || first == "unsigned") {
			typeName = first + ' ';
			tea::string nextToken = consume(TokenKind::Identf).text;
			if (nextToken == "const") {
				typeName += "const ";
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += nextToken;
		} else
			typeName = first;

		while (cur->kind == TokenKind::Star) {
			typeName += '*';
			next();
			if (cur->kind == TokenKind::Identf && cur->text == "const") {
				typeName += " const";
				next();
			}
		}

		return typeName;
	}

	std::unique_ptr<AST::ExpressionNode> Parser::parsePrimary() {
		uint32_t _line = cur->line, _column = cur->column;
		std::unique_ptr<AST::ExpressionNode> node = nullptr;

		switch (cur->kind) {
		case TokenKind::Int: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Int, cur->text);
			next();
			break;
		}

		case TokenKind::Float: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Float, cur->text);
			next();
			break;
		}

		case TokenKind::Double: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Double, cur->text);
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
			tea::string text;
			while (cur->kind == TokenKind::Identf || cur->kind == TokenKind::Scope)
				text += (cur++)->text;
			node = mknode(AST::LiteralNode, AST::ExprKind::Identf, text);
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

		while (true) {
			if (match(TokenKind::Lpar)) {
				uint32_t _line2 = cur->line, _column2 = cur->column;
				tea::vector<std::unique_ptr<AST::ExpressionNode>> args;
				if (cur->kind != TokenKind::Rpar) {
					while (true) {
						args.push(parseExpression());
						if (cur->kind == TokenKind::Comma) {
							next();
							continue;
						}
						break;
					}
				}

				consume(TokenKind::Rpar);
				node = mknodei(AST::CallNode, 2, std::move(node), std::move(args));
				continue;
			}
			
			break;
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
