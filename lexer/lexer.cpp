#include "lexer.h"

#include "tea.h"

#define pushtokex(t,v,l) container.push_back(Token{(t),std::string((v),unsigned int(l)),unsigned int(l),unsigned int((v)-src.data()),line})
#define pushtokv(t,v) pushtokex(t,v,pos-(v))
#define pushtok(t) pushtokex(t,pos,1)

#define panic tea::configuration.panic

static const char* keywords[] = {
	"return", "func", "end",
	"using", "import",
	"public", "private",
};

static bool isKeyword(const char* word, unsigned int len) {
	for (int i = 0; i < sizeof(keywords) / sizeof(const char*); i++)
		if (strncmp(word, keywords[i], len) == 0)
			return true;
	return false;
}

namespace tea {
	Lexer::Lexer() {
		pos = 0;
		line = 0;
	}

	std::vector<Token> Lexer::lex(const std::string& src) {
		std::vector<Token> container;
		if (src.empty())
			return container;

		pos = src.data();
		line = 0;

		while (*pos) {
			char c = *pos;
			if (isspace(c)) {
				if (c == '\n') line++;

			} else if (isalpha(c) || c == '_') {
				unsigned int len = 0;
				const char* start = pos;
				while (isalnum(*pos) || *pos == '_')
					pos++, len++;

				pushtokex(isKeyword(start, len) ? TOKEN_KWORD : TOKEN_IDENTF, start, len);
				continue;

			} else if (isdigit(c)) {
				const char* start = pos;
				while (isdigit(*pos))
					pos++;

				pushtokv(TOKEN_INT, start);
				continue;

			} else if (!isalnum(c) && !isspace(c)) {
				switch (c) {
				case '(':
					pushtok(TOKEN_RPAR);
					break;
				case ')':
					pushtok(TOKEN_LPAR);
					break;
				case ';':
					pushtok(TOKEN_SEMI);
					break;
				case '-':
					if (*(pos + 1) == '>') {
						pushtokex(TOKEN_ARROW, pos, 2);
						pos++;
					} else pushtok(TOKEN_SUB);
					break;
				case '+':
					pushtok(TOKEN_ADD);
					break;
				case '=':
					pushtok(TOKEN_EQ);
					break;
				case '*':
					pushtok(TOKEN_MUL);
					break;
				case '/':
					pushtok(TOKEN_DIV);
					break;
				case '!':
					pushtok(TOKEN_NOT);
					break;
				case '&':
					if (*(pos + 1) == '&') {
						pushtokex(TOKEN_AND, pos, 2);
						pos++;
						break;
					}
					TEA_FALLTHROUGH;
				case '|':
					if (*(pos + 1) == '|') {
						pushtokex(TOKEN_OR, pos, 2);
						pos++;
						break;
					}
					TEA_FALLTHROUGH;
				case ':':
					if (*(pos + 1) == ':') {
						pushtokex(TOKEN_SCOPE, pos, 2);
						pos++;
						break;
					}
					TEA_FALLTHROUGH;
				case '"': {
					const char* start = ++pos;
					while (*pos != '"' && *pos != '\n') {
						pos++;
					}

					pushtokv(TOKEN_STRING, start);
				} break;
				default:
					goto unexpected;
				}

			} else {
			unexpected:
				panic("unexpected character '%c'", c);
			}

			pos++;
		}

		return container;
	}

}
