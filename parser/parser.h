#pragma once

#include <vector>
#include <unordered_map>

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
		const Token* t;

		typedef struct {
			int nargs;
		} FunctionPrototype;

		struct {
			std::vector<std::string> imported;
			std::unordered_map<std::string, FunctionPrototype> funcs;
			FunctionPrototype* curFunc;
		} state;

		void parseBlock(const std::vector<Token>& tokens, bool stopOnEnd = false);
		void parseFunc(const std::vector<Token>& tokens, enum StorageType storage);
	};
}
