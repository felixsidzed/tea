#include "lexer.h"

#include "tea/tea.h"

#define pushtokex(t,v,l,e) container.push(Token{(t),{(v),unsigned(l)},e,unsigned(l),col,line})
#define pushtokv(t,v) pushtokex(t,v,pos-(v),0)
#define pushtok(t) pushtokex(t,pos,1,0)

static const char* keywords[] = {
	/* ORDER KWORD */

	"using", "import",
	//"macro",
	"public", "private",
	"if", "else", "elseif", "do", "while", "for", "break", "continue",
	"func", "return",
	"end",
	"var",
	"__stdcall", "__fastcall", "__cdecl", "__auto"
	//"class", "new",
};

static bool isKeyword(const char* word, unsigned int len, int* idx) {
	for (int i = 0; i < sizeof(keywords) / sizeof(const char*); i++)
		if (strlen(keywords[i]) == len && strncmp(word, keywords[i], len) == 0) {
			*idx = i;
			return true;
		}
	return false;
}

namespace tea {
	vector<Token> Lexer::tokenize(const std::string& src) {
		vector<Token> container;
		if (src.empty())
			return container;

		const char* pos = src.data();
		uint32_t line = 1;
		uint32_t col = 0;

		while (*pos) {
			char c = *pos;
			if (isspace(c)) {
				if (c == '\n') {
					pushtok(TOKEN_NEWLINE);

					line++;
					col = 0;
				}

			} else if (isalpha(c) || c == '_') {
				unsigned int len = 0;
				const char* start = pos;
				while (isalnum(*pos) || *pos == '_')
					pos++, len++;

				int i = 0;
				pushtokex(isKeyword(start, len, &i) ? TOKEN_KWORD : TOKEN_IDENTF, start, len, i);
				continue;

			} else if (isdigit(c) || (c == '-' && isdigit(*(pos + 1)))) {
				const char* start = pos;
				bool dot = false;

				if (c == '-')
					pos++;

				while (isdigit(*pos))
					pos++;

				if (*pos == '.' && isdigit(*(pos + 1))) {
					dot = 1;
					pos++;

					while (isdigit(*pos))
						pos++;
				}

				bool _float = false;
				if ((*pos == 'f' || *pos == 'F') && dot) {
					_float = true;
					pos++;
				}

				if (dot) {
					if (_float)
						pushtokv(TOKEN_FLOAT, start);
					else
						pushtokv(TOKEN_DOUBLE, start);
				} else
					pushtokv(TOKEN_INT, start);
				continue;

			} else if (!isalnum(c) && !isspace(c)) {
				switch (c) {
				case '(':
					pushtok(TOKEN_LPAR);
					break;

				case ')':
					pushtok(TOKEN_RPAR);
					break;

				case ';':
					pushtok(TOKEN_SEMI);
					break;

				case '-':
					if (*(pos + 1) == '>') {
						pushtokex(TOKEN_ARROW, pos, 2, 0);
						pos++;
					} else pushtok(TOKEN_SUB);
					break
						;
				case '+':
					pushtok(TOKEN_ADD);
					break;

				case '*':
					pushtok(TOKEN_MUL);
					break;

				case '/':
					if (*(pos + 1) == '/') {
						pos++;
						while (*pos && *pos != '\n')
							pos++;
					} else
						pushtok(TOKEN_DIV);
					break;

				case '@':
					pushtok(TOKEN_ATTR);
					break;

				case '=':
					if (*(pos + 1) == '=') {
						pushtokex(TOKEN_EQ, pos, 2, 0);
						pos++;
					}
					else
						pushtok(TOKEN_ASSIGN);
					break;

				case '!':
					if (*(pos + 1) == '=') {
						pushtokex(TOKEN_NEQ, pos, 2, 0);
						pos++;
					}
					else
						pushtok(TOKEN_NOT);
					break;

				case '<':
					if (*(pos + 1) == '=') {
						pushtokex(TOKEN_LE, pos, 2, 0);
						pos++;
					}
					else
						pushtok(TOKEN_LT);
					break;

				case '>':
					if (*(pos + 1) == '=') {
						pushtokex(TOKEN_GE, pos, 2, 0);
						pos++;
					}
					else
						pushtok(TOKEN_GT);
					break;

				case '&':
					if (*(pos + 1) == '&') {
						pushtokex(TOKEN_AND, pos, 2, 0);
						pos++;
					} else
						pushtok(TOKEN_REF);
					break;

				case '|':
					if (*(pos + 1) == '|') {
						pushtokex(TOKEN_OR, pos, 2, 0);
						pos++;
						break;
					}
					goto unexpected;

				case ':':
					if (*(pos + 1) == ':')
						pushtokex(TOKEN_SCOPE, pos, 2, 0);
					else
						pushtokex(TOKEN_COLON, pos, 1, 0);
					pos++;
					break;

				case '"': {
					const char* start = ++pos;
					std::string buffer;

					uint32_t escaped = 0;
					while (*pos != '"') {
						if (!*pos)
							TEA_PANIC("unterminated string. line %d, column %d", line, col);

						if (*pos == '\\') {
							pos++;
							switch (*pos) {
							case 'n': buffer += '\n'; break;
							case 't': buffer += '\t'; break;
							case '\\': buffer += '\\'; break;
							case '"': buffer += '"'; break;
							default:
								TEA_PANIC("invalid escape sequence \\%c. line %d, column %d", *pos, line, col);
							}
							escaped++;
						}
						else {
							if (*pos == '\n')
								TEA_PANIC("unterminated string before newline. line %d, column %d", line, col);
							buffer += *pos;
						}
						pos++;
					}

					container.push(Token{ TOKEN_STRING, { buffer.c_str(), uint32_t(pos - start) - escaped }, 0, uint32_t(pos - start) - escaped, col, line });
				} break;

				case '\'': {
					const char* start = ++pos;
					char value = 0;

					if (!*pos)
						TEA_PANIC("unterminated character literal. line %d, column %d", line, col);

					if (*pos == '\\') {
						pos++;
						switch (*pos) {
						case 'n': value = '\n'; break;
						case 't': value = '\t'; break;
						case '\\': value = '\\'; break;
						case '\'': value = '\''; break;
						case '\"': value = '\"'; break; 
						default:
							TEA_PANIC("invalid escape sequence in character literal: \\%c. line %d, column %d", *pos, line, col);
						}
						pos++;
					}
					else {
						if (*pos == '\n' || *pos == '\0')
							TEA_PANIC("unterminated character literal. line %d, column %d", line, col);

						value = *pos++;
					}

					if (*pos != '\'')
						TEA_PANIC("multi-character literal or missing closing quote. line %d, column %d", line, col);

					container.push(Token{ TOKEN_CHAR, { &value }, 0, uint32_t(pos - start), col, line });
				} break;


				case ',':
					pushtok(TOKEN_COMMA);
					break;

				case '.':
					pushtok(TOKEN_DOT);
					break;

				case '[':
					pushtok(TOKEN_LBRAC);
					break;

				case ']':
					pushtok(TOKEN_RBRAC);
					break;

				default:
					goto unexpected;
				}

			} else {
			unexpected:
				TEA_PANIC("unexpected character '%c'. line %d, column %d", c, line, col);
			}

			pos++;
			col++;
		}

		container.push(Token{ TOKEN_EOF, "", 0, 1, 0, line });
		return container;
	}

}
