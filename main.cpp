#include <iostream>

#include "lexer/token.h"
#include "lexer/lexer.h"

const char* tokenTypeToString(enum tea::TokenType type) {
	using namespace tea;
	switch (type) {
	case TOKEN_STRING: return "STRING";
	case TOKEN_INT:    return "INT";
	case TOKEN_FLOAT:  return "FLOAT";
	case TOKEN_IDENTF: return "IDENTF";
	case TOKEN_KWORD:  return "KEYWORD";
	case TOKEN_LPAR:   return "LPAR";
	case TOKEN_RPAR:   return "RPAR";
	case TOKEN_SEMI:   return "SEMICOLON";
	case TOKEN_SUB:    return "SUB";
	case TOKEN_ADD:    return "ADD";
	case TOKEN_MUL:    return "MUL";
	case TOKEN_DIV:    return "DIV";
	case TOKEN_ARROW:  return "ARROW";
	case TOKEN_EQ:     return "EQ";
	case TOKEN_NOT:    return "NOT";
	case TOKEN_AND:    return "AND";
	case TOKEN_OR:     return "OR";
	case TOKEN_SCOPE:  return "SCOPE";
	case TOKEN_EOF:    return "EOF";
	default:           return "UNKNOWN";
	}
}

void printTokens(const std::vector<tea::Token>& tokens) {
	for (const auto& token : tokens) {
		printf("Token<%s>('%s')\n", tokenTypeToString(token.type), token.value.c_str());
	}
}

int main() {
	auto src = R"(
using "io";

public func main() -> int
	io::print("Hello, World!");

	return 0;
end
)";

	tea::Lexer lexer;
	const auto& tokens = lexer.lex(src);
	printTokens(tokens);

	return 0;
}
