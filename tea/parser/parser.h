#pragma once

#include <vector>
#include <unordered_map>

#include "tea.h"
#include "node.h"
#include "lexer/token.h"

namespace tea {
	enum StorageType : uint8_t {
		STORAGE_PUBLIC,
		STORAGE_PRIVATE
	};

	class Parser {
	public:
		Parser();

		Tree parse(const std::vector<Token>& tokens);
	private:
		Tree* tree;
		std::vector<Tree*> treeHistory;

		const Token* t;

		typedef struct {
			int nargs;
		} FunctionPrototype;

		struct {
			std::vector<std::string> imported;
			std::unordered_map<std::string, FunctionPrototype> funcs;
			FunctionPrototype* curFunc = nullptr;
		} state;

		void parseBlock(const std::vector<Token>& tokens);
		void parseFunc(const std::vector<Token>& tokens, enum StorageType storage);
		std::unique_ptr<ExpressionNode> parseExpression(const std::vector<Token>& tokens);

		TEA_NORETURN unexpected();
	};
}
