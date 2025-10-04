#include <iostream>

#include "lexer/lexer.h"
#include "parser/parser.h"

//const char* tokenTypeToString(enum tea::TokenType type) {
//	using namespace tea;
//	switch (type) {
//	case TOKEN_STRING: return "STRING";
//	case TOKEN_INT:    return "INT";
//	case TOKEN_FLOAT:  return "FLOAT";
//	case TOKEN_IDENTF: return "IDENTF";
//	case TOKEN_KWORD:  return "KEYWORD";
//	case TOKEN_LPAR:   return "LPAR";
//	case TOKEN_RPAR:   return "RPAR";
//	case TOKEN_SEMI:   return "SEMICOLON";
//	case TOKEN_SUB:    return "SUB";
//	case TOKEN_ADD:    return "ADD";
//	case TOKEN_MUL:    return "MUL";
//	case TOKEN_DIV:    return "DIV";
//	case TOKEN_ARROW:  return "ARROW";
//	case TOKEN_EQ:     return "EQ";
//	case TOKEN_NOT:    return "NOT";
//	case TOKEN_AND:    return "AND";
//	case TOKEN_OR:     return "OR";
//	case TOKEN_SCOPE:  return "SCOPE";
//	case TOKEN_EOF:    return "EOF";
//	default:           return "UNKNOWN";
//	}
//}
//
//void printTokens(const std::vector<tea::Token>& tokens) {
//	for (const auto& token : tokens)
//		printf("Token<%s>('%s')\n", tokenTypeToString(token.type), token.value.c_str());
//}

void printExpressionNode(const tea::ExpressionNode* expr, int indent = 0);

void printNode(const tea::Node* node, int indent = 0) {
	using namespace tea;
	if (!node) return;

	for (int i = 0; i < indent; ++i)
		printf("  ");

	switch (node->type) {
	case NODE_UsingNode: {
		auto* usingNode = static_cast<const UsingNode*>(node);
		printf("UsingNode: name = %s\n", usingNode->name.c_str());
		break;
	}
	case NODE_FunctionNode: {
		auto* funcNode = static_cast<const FunctionNode*>(node);
		printf("FunctionNode: storage = %u\n", funcNode->storage);
		for (const auto& child : funcNode->body)
			printNode(child.get(), indent + 1);
		break;
	}
	case NODE_ReturnNode: {
		auto* returnNode = static_cast<const ReturnNode*>(node);
		printf("ReturnNode:\n");
		printExpressionNode(returnNode->value.get(), indent + 1);
		break;
	}
	default:
		printf("Unknown node type\n");
	}
}

void printExpressionNode(const tea::ExpressionNode* expr, int indent) {
	using namespace tea;
	if (!expr) return;

	for (int i = 0; i < indent; ++i)
		printf("  ");

	const char* typeStr = "Unknown";
	switch (expr->type) {
		case EXPR_INT:    typeStr = "INT"; break;
		case EXPR_FLOAT:  typeStr = "FLOAT"; break;
		case EXPR_STRING: typeStr = "STRING"; break;
		case EXPR_IDENTF: typeStr = "IDENTF"; break;
	}

	printf("ExpressionNode<%s>(\"%s\")\n", typeStr, expr->value.c_str());
	if (expr->left)  printExpressionNode(expr->left, indent + 1);
	if (expr->right) printExpressionNode(expr->right, indent + 1);
}

void printTree(const tea::Tree& tree) {
	for (const auto& node : tree)
		printNode(node.get(), 0);
}

int main() {
	auto src = R"(
using "io";

public func main() -> int
	return 1;
end
)";

	tea::Lexer lexer;
	const auto& tokens = lexer.lex(src);
	//printTokens(tokens);

	tea::Parser parser;
	const auto& root = parser.parse(tokens);
	printTree(root);

	return 0;
}
