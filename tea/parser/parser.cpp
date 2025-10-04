#include "parser.h"

#include "tea.h"
#include "lexer/token.h"

#define pushnode(T,...) { \
	auto node = std::make_unique<T>(__VA_ARGS__); \
	node->type = NODE_##T; \
	node->line, node->column = t->line, t->column;\
	tree->push_back(std::move(node)); \
}

#define pushtree(t,...) { \
	auto node = std::make_unique<t>(__VA_ARGS__); \
	node->type = NODE_##t; \
	auto body = &node->body; \
	treeHistory.push_back(tree); \
	tree->push_back(std::move(node)); \
	tree = body; \
}
#define poptree() { \
	if (TEA_UNLIKELY(treeHistory.empty())) \
		__debugbreak(); \
	tree = treeHistory.back(); \
	treeHistory.pop_back(); \
}

#define advance() { \
	if ((t++)->type == TOKEN_EOF) \
		TEA_PANIC("unexpected EOF. line %d, column %d", (--t)->line, t->column); \
}
#define expect(tt) _expect(t,tt)

namespace tea {
	std::unordered_map<TEA_TOKENVAL, enum Type> name2type = {
		{"int", TYPE_INT},
		{"float", TYPE_FLOAT},
		{"double", TYPE_DOUBLE},
		{"char", TYPE_CHAR},
		{"string", TYPE_STRING},
		{"void", TYPE_VOID},
		{"bool", TYPE_BOOL}
	};

	static inline const TEA_TOKENVAL& _expect(const Token*& t, enum TokenType expected) {
		if (t->type != expected)
			TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
		advance();
		return (t - 1)->value;
	}

	static inline bool isCC(enum KeywordType tt) {
		return tt == KWORD_FASTCC || tt == KWORD_STDCC || tt == KWORD_CCC;
	}

	Parser::Parser() {
		t = 0;
		tree = nullptr;
	}

	Tree Parser::parse(const std::vector<Token>& tokens) {
		t = tokens.data();

		Tree root;
		tree = &root;
		treeHistory = { &root };

		imported.clear();
		funcs.clear();

		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD:
				switch (t->extra) {
				case KWORD_USING: {
					advance();
					const std::string& name = expect(TOKEN_STRING);
					auto it = std::find(imported.begin(), imported.end(), name);
					if (it != imported.end())
						TEA_PANIC("module re-import. line %d, column %d", t->line, t->column);
					imported.push_back(name);
					expect(TOKEN_SEMI);
					pushnode(UsingNode, name);
				} break;

				case KWORD_PUBLIC: {
					advance();
					if (t->extra == KWORD_FUNC || isCC((enum KeywordType)t->extra))
						parseFunc(STORAGE_PUBLIC);
					else
						unexpected();
				} break;

				case KWORD_PRIVATE: {
					advance();
					if (t->extra == KWORD_FUNC || isCC((enum KeywordType)t->extra))
						parseFunc(STORAGE_PRIVATE);
					else
						unexpected();
				} break;

				case KWORD_IMPORT: {
					advance();
					enum CallingConvention cc = DEFAULT_CC;

					if (isCC((enum KeywordType)t->extra)) {
						switch (t->extra) {
						case KWORD_STDCC:	cc = CC_STD;	break;
						case KWORD_FASTCC:	cc = CC_FAST;	break;
						case KWORD_CCC:		cc = CC_C;		break;
						}
						advance();
					}
					else
						unexpected();

					if (t->type != TOKEN_KWORD || t->extra != KWORD_FUNC)
						unexpected();
					advance();

					const TEA_TOKENVAL& name = expect(TOKEN_IDENTF);
					auto it = std::find(funcs.begin(), funcs.end(), name);
					if (it != funcs.end())
						TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
					funcs.push_back(name);
					expect(TOKEN_LPAR);

					std::vector<std::pair<enum Type, std::string>> args;
					if (t->type != TOKEN_RPAR) {
						do {
							const TEA_TOKENVAL& typeName = expect(TOKEN_IDENTF);
							auto typeIt = name2type.find(typeName);
							if (typeIt == name2type.end())
								TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.c_str(), (t - 1)->line, (t - 1)->column);
							const TEA_TOKENVAL& argName = expect(TOKEN_IDENTF);
							args.emplace_back(typeIt->second, argName);
						} while (t->type == TOKEN_COMMA && t++);
					}

					expect(TOKEN_RPAR);
					expect(TOKEN_ARROW);
					const TEA_TOKENVAL& returnType = expect(TOKEN_IDENTF);
					auto it2 = name2type.find(returnType);
					if (it2 == name2type.end())
						TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.c_str(), (t - 1)->line, (t - 1)->column);

					expect(TOKEN_SEMI);
					pushnode(FunctionImportNode, cc, name, args, it2->second);
				} break;

				default:
					unexpected();
				} break;
			default:
				unexpected();
			}
		}

		return std::move(root);
	}

	std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
		switch (t->type) {
		case TOKEN_INT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_INT, t->value);
			advance();
			return node;
		}
		case TOKEN_FLOAT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_FLOAT, t->value);
			advance();
			return node;
		}
		case TOKEN_DOUBLE: {
			auto node = std::make_unique<ExpressionNode>(EXPR_DOUBLE, t->value);
			advance();
			return node;
		}
		case TOKEN_STRING: {
			auto node = std::make_unique<ExpressionNode>(EXPR_STRING, t->value);
			advance();
			return node;
		}
		case TOKEN_IDENTF: {
			std::vector<std::string> scope;
			TEA_TOKENVAL value = t->value;
			advance();

			while (t->type == TOKEN_SCOPE) {
				advance();
				if (t->type != TOKEN_IDENTF) unexpected();
				scope.push_back(value);
				value = t->value;
				advance();
			}

			if (t->type == TOKEN_LPAR) {
				advance();
				std::vector<std::unique_ptr<ExpressionNode>> args;
				if (t->type != TOKEN_RPAR) {
					while (true) {
						args.push_back(parseExpression());
						if (t->type == TOKEN_COMMA) {
							advance();
							continue;
						}
						break;
					}
				}
				expect(TOKEN_RPAR);
				return std::make_unique<CallNode>(scope, value, std::move(args));
			} else
				return std::make_unique<ExpressionNode>(EXPR_IDENTF, value);
		}
		case TOKEN_LPAR: {
			advance();
			auto expr = parseExpression();
			expect(TOKEN_RPAR);
			return expr;
		}
		default:
			unexpected();
		}
		return nullptr;
	}

	static inline int getPrecedence(enum TokenType type) {
		switch (type) {
		case TOKEN_ADD:
		case TOKEN_SUB:
			return 1;
		case TOKEN_MUL:
		case TOKEN_DIV:
			return 2;
		default:
			return -1;
		}
	}

	static const std::unordered_map<enum TokenType, enum ExpressionType> tt2et = {
		{TOKEN_ADD, EXPR_ADD},
		{TOKEN_SUB, EXPR_SUB},
		{TOKEN_DIV, EXPR_DIV},
		{TOKEN_MUL, EXPR_MUL},
		{TOKEN_IDENTF, EXPR_IDENTF},
	};

	std::unique_ptr<ExpressionNode> Parser::parseRhs(int exprPrec, std::unique_ptr<ExpressionNode> lhs) {
		while (true) {
			int tokPrec = getPrecedence(t->type);

			if (tokPrec < exprPrec)
				return lhs;

			enum TokenType tt = t->type;
			advance();

			auto rhs = parsePrimary();

			int nextPrec = getPrecedence(t->type);
			if (tokPrec < nextPrec)
				rhs = parseRhs(tokPrec + 1, std::move(rhs));

			lhs = std::make_unique<ExpressionNode>(tt2et.at(tt), "", std::move(lhs), std::move(rhs));
		}
	}

	std::unique_ptr<ExpressionNode> Parser::parseExpression() {
		auto lhs = parsePrimary();
		return parseRhs(0, std::move(lhs));
	}

	void Parser::parseBlock() {
		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD:
				switch (t->extra) {
				case KWORD_END: {
					if (treeHistory.empty())
						unexpected();
					advance();
					poptree();
					return;
				}

				case KWORD_RETURN: {
					advance();
					pushnode(ReturnNode, parseExpression());
					expect(TOKEN_SEMI);
					break;
				}
				case KWORD_VAR: {
					advance();
					const TEA_TOKENVAL& name = expect(TOKEN_IDENTF);
					expect(TOKEN_COLON);
					enum Type dataType = name2type[expect(TOKEN_IDENTF)];

					std::unique_ptr<ExpressionNode> value = nullptr;
					if (t->type != TOKEN_SEMI) {
						expect(TOKEN_EQ);
						value = parseExpression();
					}
					expect(TOKEN_SEMI);

					pushnode(VariableNode, name, dataType, std::move(value));
					break;
				}

				default:
					goto unexpected;
				}
				break;

			case TOKEN_IDENTF: {
				tree->push_back(parseExpression());
				expect(TOKEN_SEMI);
			} break;

			default:
			unexpected:
				unexpected();
			}
		}
	}

	void Parser::parseFunc(enum StorageType storage) {
		enum CallingConvention cc = DEFAULT_CC;

		if (isCC((enum KeywordType)t->extra)) {
			switch (t->extra) {
			case KWORD_STDCC:	cc = CC_STD;	break;
			case KWORD_FASTCC:	cc = CC_FAST;	break;
			case KWORD_CCC:		cc = CC_C;		break;
			}
			advance();
		}
		advance();

		const TEA_TOKENVAL& name = expect(TOKEN_IDENTF);
		auto it = std::find(funcs.begin(), funcs.end(), name);
		if (it != funcs.end())
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
		funcs.push_back(name);
		expect(TOKEN_LPAR);

		std::vector<std::pair<enum Type, std::string>> args;
		if (t->type != TOKEN_RPAR) {
			do {
				const TEA_TOKENVAL& typeName = expect(TOKEN_IDENTF);
				auto typeIt = name2type.find(typeName);
				if (typeIt == name2type.end())
					TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.c_str(), (t - 1)->line, (t - 1)->column);
				const TEA_TOKENVAL& argName = expect(TOKEN_IDENTF);
				args.emplace_back(typeIt->second, argName);
			} while (t->type == TOKEN_COMMA && t++);
		}

		expect(TOKEN_RPAR);
		expect(TOKEN_ARROW);
		const TEA_TOKENVAL& returnType = expect(TOKEN_IDENTF);
		auto it2 = name2type.find(returnType);
		if (it2 == name2type.end())
			TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.c_str(), (t - 1)->line, (t - 1)->column);
		pushtree(FunctionNode, storage, cc, name, args, it2->second);
		parseBlock();
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
	}
}
