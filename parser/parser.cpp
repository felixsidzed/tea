#include "parser.h"

#include "tea.h"
#include "lexer/token.h"

#define pushnode(t,...) { \
	auto node = std::make_unique<t>(__VA_ARGS__); \
	node->type = NODE_##t; \
	tree->push_back(std::move(node)); \
}
#define pushtree(t,...) { \
	auto node = std::make_unique<t>(__VA_ARGS__); \
	node->type = NODE_##t; \
	auto body = &node->body; \
	tree->push_back(std::move(node)); \
	tree = body; \
}

#define advance() { \
	if ((t++)->type == TOKEN_EOF) \
		TEA_PANIC("unexpected EOF. line %d, column %d", (--t)->line, t->pos); \
}
#define expect(tt) _expect(t,tt)

namespace tea {
	static inline const std::string& _expect(const Token*& t, enum TokenType expected) {
		if (t->type != expected)
			TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->pos);
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

		state.imported.clear();
		state.funcs.clear();
		state.curFunc = nullptr;

		parseBlock(tokens);

		return std::move(root);
	}

	void Parser::parseBlock(const std::vector<Token>& tokens, bool stopOnEnd) {
		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD:
				switch (t->extra) {
				case KWORD_USING: {
					advance();
					const std::string& name = expect(TOKEN_STRING);
					auto it = std::find(state.imported.begin(), state.imported.end(), name);
					if (it != state.imported.end())
						TEA_PANIC("module re-import. line %d, column %d", t->line, t->pos);
					state.imported.push_back(name);
					expect(TOKEN_SEMI);
					pushnode(UsingNode, name);
				} break;

				case KWORD_PUBLIC: {
					advance();
					if (t->extra == KWORD_FUNC) {
						advance();
						parseFunc(tokens, STORAGE_PUBLIC);
					} else
						goto unexpected;
				} break;

				case KWORD_PRIVATE: {
					advance();
					if (t->extra == KWORD_FUNC) {
						advance();
						parseFunc(tokens, STORAGE_PRIVATE);
					} else
						goto unexpected;
				} break;

				case KWORD_END: {
					advance();
					return;
				}

				default:
					goto unexpected;
				}
				break;
			default:
			unexpected:
				TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->pos);
			}
		}
	}

	void Parser::parseFunc(const std::vector<Token>& tokens, enum StorageType storage) {
		const std::string& name = expect(TOKEN_IDENTF);
		if (state.funcs.contains(name))
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->pos);
		state.funcs[name] = {};
		state.curFunc = &state.funcs[name];
		expect(TOKEN_RPAR);
		// TODO: arguments
		expect(TOKEN_LPAR);
		expect(TOKEN_ARROW);
		const std::string& returnType = expect(TOKEN_IDENTF);
		pushtree(FunctionNode, storage);
		parseBlock(tokens, true);
	}
}
