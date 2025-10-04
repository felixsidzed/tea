#pragma once

#include <vector>
#include <string>

#include "token.h"
#include "tea/vector.h"

namespace tea {
	namespace Lexer {
		vector<Token> tokenize(const std::string& source);
	}
}
