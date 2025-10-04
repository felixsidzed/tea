#pragma once

#include "tea.h"

namespace tea {
	enum TokenType : uint8_t {
		TOKEN_STRING,

		TOKEN_INT,
		TOKEN_FLOAT,

		TOKEN_IDENTF,
		TOKEN_KWORD,

		TOKEN_LPAR,
		TOKEN_RPAR,
		TOKEN_SEMI,

		TOKEN_SUB,
		TOKEN_ADD,
		TOKEN_MUL,
		TOKEN_DIV,
		TOKEN_ARROW,
		TOKEN_EQ,
		TOKEN_NOT,
		TOKEN_AND,
		TOKEN_OR,
		TOKEN_SCOPE,

		TOKEN_EOF
	};

	enum KeywordType : uint8_t {
		/* ORDER KWORD */

		KWORD_USING, //KWORD_IMPORT,
		KWORD_MACRO,
		KWORD_PUBLIC, KWORD_PRIVATE,
		/*KWORD_IF, KWORD_ELSEIF, KWORD_ELSE, KWORD_WHILE, KWORD_FOR, KWORD_BREAK, KWORD_CONTINUE,*/
		KWORD_FUNC, KWORD_RETURN,
		KWORD_END,
		//KWORD_VAR,
		//KWORD_STDCC, KWORD_FASTCC, KWORD_CCC,
		//KWORD_CLASS, KWORD_NEW, KWORD_CTOR, KWORD_DTOR,
	};

	struct Token {
		enum TokenType type;
		const TEA_TOKENVAL value;
		int extra;

		unsigned int length;
		unsigned int pos;
		unsigned int line;
	};
}
