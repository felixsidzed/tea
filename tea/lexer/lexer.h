#pragma once

#include <vector>
#include <string>

#include "token.h"

namespace tea {
	namespace Lexer {
		std::vector<Token> tokenize(const std::string& source);
	}
}
