#pragma once

#include <string>

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

	struct Token {
		enum TokenType type;
		const std::string value;

		unsigned int length;
		unsigned int pos;
		unsigned int line;
	};
}
