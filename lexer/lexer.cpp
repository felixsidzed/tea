#include "lexer.h"

#include "tea.h"

#define pushtokex(t,v,l,e) container.push_back(Token{(t),{(v),unsigned int(l)},e,unsigned int(l),unsigned int((v)-src.data()),line})
#define pushtokv(t,v) pushtokex(t,v,pos-(v),0)
#define pushtok(t) pushtokex(t,pos,1,0)

static const char* keywords[] = {
	/* ORDER KWORD */

	"using", "func", //"import", "macro",
	"public", "private",
	/*"if", "elseif", "else", "while", "for", "break", "continue",*/ "return",
	//"var",
	//"__stdcall", "__fastcall", "__cdecl",
	//"class", "new", "constructor", "deconstructor",
};

static bool isKeyword(const char* word, unsigned int len, int* idx) {
	for (int i = 0; i < sizeof(keywords) / sizeof(const char*); i++)
		if (strncmp(word, keywords[i], len) == 0) {
			*idx = i;
			return true;
		}
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

				int i = 0;
				pushtokex(isKeyword(start, len, &i) ? TOKEN_KWORD : TOKEN_IDENTF, start, len, i);
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
						pushtokex(TOKEN_ARROW, pos, 2, 0);
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
						pushtokex(TOKEN_AND, pos, 2, 0);
						pos++;
						break;
					}
					TEA_FALLTHROUGH;
				case '|':
					if (*(pos + 1) == '|') {
						pushtokex(TOKEN_OR, pos, 2, 0);
						pos++;
						break;
					}
					TEA_FALLTHROUGH;
				case ':':
					if (*(pos + 1) == ':') {
						pushtokex(TOKEN_SCOPE, pos, 2, 0);
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
				TEA_PANIC("unexpected character '%c'", c);
			}

			pos++;
		}

		return container;
	}

}
