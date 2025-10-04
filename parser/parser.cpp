#include "parser.h"

#include "tea.h"
#include "lexer/token.h"

#define pushnode(t,n,...) tree->push_back(t(##__VA_ARGS__))

#define advance() (t++)

#define expect(tt) _expect(t,tt)

namespace tea {
	static inline const std::string& _expect(const Token*& t, enum TokenType tt) {
		if (t->type != tt)
			TEA_PANIC("unexpected token '%s'", t->value.c_str());
		return (t++)->value;
	}

	Parser::Parser() {
		t = 0;
		tree = nullptr;
	}

	Tree Parser::parse(const std::vector<Token>& tokens) {
		t = tokens.data();

		Tree root;
		tree = &root;

		const Token* tokensEnd = tokens.data() + tokens.size();
		while (t < tokensEnd) {
			switch (t->type) {
			case TOKEN_KWORD:
				switch (t->extra) {
				case KWORD_USING:
					advance();
					expect(TOKEN_STRING);
					expect(TOKEN_SEMI);
				}
				TEA_FALLTHROUGH;
			default:
				TEA_PANIC("unexpected token '%s'", t->value.c_str());
			}

			t++;
		}

		return *tree;
	}
}
