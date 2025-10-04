#pragma once

#include <vector>
#include <string>

#include "token.h"

namespace tea {
	class Lexer {
	public:
		Lexer();

		std::vector<Token> lex(const std::string& source);
	private:
		const char* pos;
		uint32_t line;
		uint32_t col;
	};
}
