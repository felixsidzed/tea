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
	static const std::unordered_map<std::string, enum Type> name2type = {
		{"int", TYPE_INT},
		{"float", TYPE_FLOAT},
		{"double", TYPE_DOUBLE},
		{"char", TYPE_CHAR},
		{"string", TYPE_STRING},
		{"void", TYPE_VOID},
		{"bool", TYPE_BOOL}
	};

	static inline const std::string& _expect(const Token*& t, enum TokenType expected) {
		if (t->type != expected)
			TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
		advance();
		return (t - 1)->value;
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

		state.imported.clear();
		state.funcs.clear();
		state.curFunc = nullptr;

		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD:
				switch (t->extra) {
				case KWORD_USING: {
					advance();
					const std::string& name = expect(TOKEN_STRING);
					auto it = std::find(state.imported.begin(), state.imported.end(), name);
					if (it != state.imported.end())
						TEA_PANIC("module re-import. line %d, column %d", t->line, t->column);
					state.imported.push_back(name);
					expect(TOKEN_SEMI);
					pushnode(UsingNode, name);
				} break;

				case KWORD_PUBLIC: {
					advance();
					if (t->extra == KWORD_FUNC) {
						advance();
						parseFunc(STORAGE_PUBLIC);
					}
					else
						goto unexpected;
				} break;

				case KWORD_PRIVATE: {
					advance();
					if (t->extra == KWORD_FUNC) {
						advance();
						parseFunc(STORAGE_PRIVATE);
					}
					else
						goto unexpected;
				} break;

				default:
				unexpected:
					unexpected();
				}
			}
		}

		return std::move(root);
	}

	std::unique_ptr<ExpressionNode> Parser::parseExpression() {
		switch (t->type) {
		case TOKEN_INT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_INT, t->value);
			advance();
			return node;

		} case TOKEN_FLOAT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_FLOAT, t->value);
			advance();
			return node;

		} case TOKEN_DOUBLE: {
			auto node = std::make_unique<ExpressionNode>(EXPR_DOUBLE, t->value);
			advance();
			return node;

		} case TOKEN_STRING: {
			auto node = std::make_unique<ExpressionNode>(EXPR_STRING, t->value);
			advance();
			return node;

		} case TOKEN_IDENTF: {
			std::vector<std::string> scope;
			std::string value = t->value;
			advance();

			while (t->type == TOKEN_SCOPE) {
				advance();

				if (t->type != TOKEN_IDENTF)
					unexpected();
				scope.push_back(value);
				value = t->value;
				advance();
			}

			if (t->type == TOKEN_RPAR) {
				advance();
				std::vector<std::unique_ptr<ExpressionNode>> args;

				if (t->type != TOKEN_LPAR) {
					while (true) {
						args.push_back(parseExpression());
						if (t->type == TOKEN_COMMA) {
							advance();
							continue;
						}
						break;
					}
				}

				expect(TOKEN_LPAR);
				return std::make_unique<CallNode>(scope, value, std::move(args));
			} else
				return std::make_unique<ExpressionNode>(EXPR_IDENTF, value);

		} default:
			unexpected();
		}
		return nullptr;
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

				default:
					goto unexpected;
				}
				break;

			case TOKEN_IDENTF: {
				tree->push_back(parseExpression());
			} break;

			default:
			unexpected:
				unexpected();
			}
		}
	}

	void Parser::parseFunc(enum StorageType storage) {
		const std::string& name = expect(TOKEN_IDENTF);
		if (state.funcs.contains(name))
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
		state.funcs[name] = {};
		state.curFunc = &state.funcs[name];
		expect(TOKEN_RPAR);
		// TODO: arguments
		expect(TOKEN_LPAR);
		expect(TOKEN_ARROW);
		const std::string& returnType = expect(TOKEN_IDENTF);
		pushtree(FunctionNode, storage, name, name2type.at(returnType));
		parseBlock();
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
	}
}
