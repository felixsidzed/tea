#include "Lexer.h"

#include "common/tea.h"

#define pushtokex(t, v, l, e) tokens.push(Token((t), (e), tea::string((v), (l)), line, col ))
#define pushtokv(t, v) pushtokex(t, v, pos - (v), 0)
#define pushtok(t) pushtokex(t, pos, 1, 0)

static const char* keywords[] = {
	"public", "private",
	"using", "import",
	//"macro",
	"if", "else", "elseif", "do", "while", //"for", "break", "continue",
	"func", "return", "end",
	"var",
	"__stdcall", "__fastcall", "__cdecl", "__auto",
	//"class", "new",
};

static bool isKeyword(const char* word, unsigned int len, int* idx) {
	for (int i = 0; i < sizeof(keywords) / sizeof(const char*); i++)
		if (strlen(keywords[i]) == len && !strncmp(word, keywords[i], len)) {
			*idx = i;
			return true;
		}
	return false;
}

static char unescape(const char*& p) {
	p++;
	char c = *p;

	switch (c) {
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case 'f': return '\f';
	case 'b': return '\b';
	case 'a': return '\a';
	case '\\':return '\\';
	case '\'':return '\'';
	case '"': return '"';

	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7': {
		int v = 0, count = 0;
		while (*p >= '0' && *p <= '7' && count < 3) {
			v = (v << 3) | (*p - '0');
			p++; count++;
		}
		p--;
		return (char)v;
	}

	case 'x': {
		p++;
		int v = 0;
		if (!isxdigit(*p)) return 0;
		while (isxdigit(*p)) {
			v = (v << 4) | (isdigit(*p) ? *p - '0' : tolower(*p) - 'a' + 10);
			p++;
		}
		p--;
		return (char)v;
	}

	default:
		return c;
	}
}

namespace tea::frontend {

	tea::vector<Token> lex(const tea::string& src) {
		tea::vector<Token> tokens;
		if (src.empty())
			return tokens;

		const char* pos = src.data();
		uint32_t line = 1;
		uint32_t col = 1;

		while (*pos > 0) {
			char c = *pos;
			if (isspace(c)) {
				if (c == '\n')
					line++, col = 1;
			}
			else if (isalpha(c) || c == '_') {
				unsigned int len = 0;
				const char* start = pos;
				while (*pos > 0 && (isalnum(*pos) || *pos == '_'))
					pos++, len++, col++;

				int i = 0;
				if (isKeyword(start, len, &i))
					pushtokex(TokenKind::Keyword, start, len, (uint32_t)i);
				else
					pushtokex(TokenKind::Identf, start, len, 0);
				continue;
			}
			else if (isdigit(c) || (c == '-' && isdigit(*(pos + 1)))) {
				const char* start = pos;

				if (c == '-')
					pos++, col++;

				while (isdigit(*pos))
					pos++, col++;

				if (c == '0' && (*pos == 'x' || *pos == 'X')) {
					pos += 2; col += 2;
					while (isxdigit(*pos))
						pos++, col++;
					pushtokv(TokenKind::Int, start);
					continue;
				}

				bool dot = false;
				if (*pos == '.' && isdigit(*(pos + 1))) {
					dot = true;
					pos++, col++;
					while (isdigit(*pos))
						pos++, col++;
				}

				bool _float = false;
				if ((*pos == 'f' || *pos == 'F') && dot) {
					_float = true;
					pos++, col++;
				}

				if (dot)
					pushtokv(_float ? TokenKind::Float : TokenKind::Double, start);
				else
					pushtokv(TokenKind::Int, start);
				continue;
			}
			else {
				switch (c) {
				case '(': pushtok(TokenKind::Lpar); break;
				case ')': pushtok(TokenKind::Rpar); break;
				case ';': pushtok(TokenKind::Semicolon); break;
				case '+': pushtok(TokenKind::Add); break;
				case '*': pushtok(TokenKind::Star); break;
				case ',': pushtok(TokenKind::Comma); break;
				case '.': pushtok(TokenKind::Dot); break;
				case '[': pushtok(TokenKind::Lbrac); break;
				case ']': pushtok(TokenKind::Rbrac); break;
				case '~': pushtok(TokenKind::Tilde); break;
				case '^': pushtok(TokenKind::Bxor); break;
				case '@': pushtok(TokenKind::At); break;

				case '-':
					if (*(pos + 1) == '>') {
						pushtokex(TokenKind::Arrow, pos, 2, 0);
						pos++, col++;
					} else pushtok(TokenKind::Sub);
					break;

				case '/':
					if (*(pos + 1) == '/') {
						pos++, col++;
						while (*pos && *pos != '\n')
							pos++, col++;
					}
					else pushtok(TokenKind::Div);
					break;

				case '=':
					if (*(pos + 1) == '=') {
						pushtokex(TokenKind::Eq, pos, 2, 0);
						pos++, col++;
					} else pushtok(TokenKind::Assign);
					break;

				case '!':
					if (*(pos + 1) == '=') {
						pushtokex(TokenKind::Neq, pos, 2, 0);
						pos++, col++;
					}
					else pushtok(TokenKind::Not);
					break;

				case '<':
					if (*(pos + 1) == '=') {
						pushtokex(TokenKind::Le, pos, 2, 0);
						pos++, col++;
					}
					else if (*(pos + 1) == '<') {
						pushtokex(TokenKind::Shl, pos, 2, 0);
						pos++, col++;
					}
					else pushtok(TokenKind::Lt);
					break;

				case '>':
					if (*(pos + 1) == '=') {
						pushtokex(TokenKind::Ge, pos, 2, 0);
						pos++, col++;
					}
					else if (*(pos + 1) == '>') {
						pushtokex(TokenKind::Shr, pos, 2, 0);
						pos++, col++;
					}
					else pushtok(TokenKind::Gt);
					break;

				case '&':
					if (*(pos + 1) == '&') {
						pushtokex(TokenKind::And, pos, 2, 0);
						pos++, col++;
					}
					else pushtok(TokenKind::Amp);
					break;

				case '|':
					if (*(pos + 1) == '|') {
						pushtokex(TokenKind::Or, pos, 2, 0);
						pos++, col++;
					}
					else pushtok(TokenKind::Bor);
					break;

				case ':':
					if (*(pos + 1) == ':') {
						pushtokex(TokenKind::Scope, pos, 2, 0);
						pos++, col++;
					}
					else pushtokex(TokenKind::Colon, pos, 1, 0);
					break;

				case '"': {
					const char* start = ++pos;
					std::string buffer;
					while (*pos != '"') {
						if (!*pos)
							TEA_PANIC("unterminated string. line %d, column %d", line, col);
						if (*pos == '\\')
							buffer += unescape(pos);
						else
							buffer += *pos;
						if (*pos++ == '\n')
							col = 1, line++;
						else
							col++;
					}
					tokens.push(Token(TokenKind::String, 0, tea::string(buffer.c_str(), buffer.size()), line, col));
				} break;

				case '\'': {
					const char* start = ++pos;
					char value = 0;
					if (!*pos)
						TEA_PANIC("unterminated char");
					if (*pos == '\\')
						value = unescape(pos);
					else
						value = *pos;
					pos++; col++;
					if (*pos != '\'')
						TEA_PANIC("bad char literal");
					tokens.push(Token(TokenKind::Char, 0, tea::string(&value, 1), line, col));
				} break;

				default:
					TEA_PANIC("unexpected character");
				}
			}

			pos++; col++;
		}

		pushtok(TokenKind::EndOfFile);
		return tokens;
	}

} // namespace tea::frontend
