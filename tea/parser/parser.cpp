#include "parser.h"

#include "tea.h"
#include "lexer/token.h"

#define pushnode(T,...) { \
	auto node = std::make_unique<T>(__VA_ARGS__); \
	node->type = tnode(T); \
	node->line = t->line; node->column = t->column; \
	tree->push_back(std::move(node)); \
}

#define pushtree(T,...) { \
	auto node = std::make_unique<T>(__VA_ARGS__); \
	node->type = tnode(T); \
	node->line = t->line; node->column = t->column; \
	auto body = &node->body; \
	treeHistory.push_back(tree); \
	tree->push_back(std::move(node)); \
	tree = body; \
}
#define poptree() { \
	if (TEA_UNLIKELY(treeHistory.empty())) \
		__debugbreak(); \
	tree = treeHistory.back(); \
	treeHistory.pop_back(); \
}

#define advance() { \
	if ((t++)->type == TOKEN_EOF) \
		TEA_PANIC("unexpected EOF. line %d, column %d", (--t)->line, t->column); \
}
#define expect(tt) _expect(t,tt)

namespace tea {
	std::unordered_map<std::string, enum Type> name2type = {
		{"int", TYPE_INT},
		{"float", TYPE_FLOAT},
		{"double", TYPE_DOUBLE},
		{"char", TYPE_CHAR},
		{"string", TYPE_STRING},
		{"void", TYPE_VOID},
		{"bool", TYPE_BOOL}
	};

	std::unordered_map<std::string, enum Attribute> name2attr = {
		{"inline", ATTR_INLINE},
	};

	static inline const std::string& _expect(const Token*& t, enum TokenType expected) {
		if (t->type != expected)
			TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
		advance();
		return (t - 1)->value;
	}

	static inline bool isCC(enum KeywordType tt) {
		return tt == KWORD_FASTCC || tt == KWORD_STDCC || tt == KWORD_CCC;
	}

	Parser::Parser() {
		t = 0;
		tree = nullptr;
	}

	Tree Parser::parse(const std::vector<Token>& tokens) {
		t = tokens.data();

		Tree root;
		tree = &root;
		treeHistory = { &root };

		imported.clear();
		funcs.clear();

		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD: {
				switch (t->extra) {
				case KWORD_USING: {
					advance();
					const std::string& name = expect(TOKEN_STRING);
					auto it = std::find(imported.begin(), imported.end(), name);
					if (it != imported.end())
						TEA_PANIC("module re-import. line %d, column %d", t->line, t->column);
					imported.push_back(name);
					expect(TOKEN_SEMI);
					pushnode(UsingNode, name);
				} break;

				case KWORD_PRIVATE:
				case KWORD_PUBLIC: {
					if ((t + 1)->type == TOKEN_KWORD && (t + 1)->extra == KWORD_VAR) {
						enum StorageType storage;
						if (t->extra == KWORD_PUBLIC)
							storage = STORAGE_PUBLIC;
						else if (t->extra == KWORD_PRIVATE)
							storage = STORAGE_PRIVATE;
						else
							unexpected();
						t++;
						advance();

						const std::string& name = expect(TOKEN_IDENTF);
						expect(TOKEN_COLON);
						
						const std::string& dataType = expect(TOKEN_IDENTF);
						auto it = name2type.find(dataType);
						if (it == name2type.end())
							TEA_PANIC("unknown type '%s'. line %d, column %d", dataType.c_str(), (t - 1)->line, (t - 1)->column);

						std::unique_ptr<ExpressionNode> value = nullptr;
						if (t->type != TOKEN_SEMI) {
							expect(TOKEN_ASSIGN);
							value = parseExpression();
						}
						expect(TOKEN_SEMI);

						pushnode(GlobalVariableNode, storage, name, it->second, std::move(value));
					} else
						parseFuncFull();
				} break;

				case KWORD_IMPORT: {
					advance();
					enum CallingConvention cc = DEFAULT_CC;

					if (isCC((enum KeywordType)t->extra)) {
						switch (t->extra) {
						case KWORD_STDCC:	cc = CC_STD;	break;
						case KWORD_FASTCC:	cc = CC_FAST;	break;
						case KWORD_CCC:		cc = CC_C;		break;
						}
						advance();
					}
					else
						unexpected();

					if (t->type != TOKEN_KWORD || t->extra != KWORD_FUNC)
						unexpected();
					advance();

					const std::string& name = expect(TOKEN_IDENTF);
					auto it = std::find(funcs.begin(), funcs.end(), name);
					if (it != funcs.end())
						TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
					funcs.push_back(name);
					expect(TOKEN_LPAR);

					std::vector<std::pair<enum Type, std::string>> args;
					if (t->type != TOKEN_RPAR) {
						do {
							const std::string& typeName = expect(TOKEN_IDENTF);
							auto typeIt = name2type.find(typeName);
							if (typeIt == name2type.end())
								TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.c_str(), (t - 1)->line, (t - 1)->column);
							const std::string& argName = expect(TOKEN_IDENTF);
							args.emplace_back(typeIt->second, argName);
						} while (t->type == TOKEN_COMMA && t++);
					}

					expect(TOKEN_RPAR);
					expect(TOKEN_ARROW);
					const std::string& returnType = expect(TOKEN_IDENTF);
					auto it2 = name2type.find(returnType);
					if (it2 == name2type.end())
						TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.c_str(), (t - 1)->line, (t - 1)->column);

					expect(TOKEN_SEMI);
					pushnode(FunctionImportNode, cc, name, args, it2->second);
				} break;

				default:
					unexpected();
				} break;
			} break;

			case TOKEN_ATTR: {
				advance();
				std::vector<enum Attribute> attrs;
				while (true) {
					const std::string& attrName = expect(TOKEN_IDENTF);
					auto it = name2attr.find(attrName);
					if (it == name2attr.end())
						TEA_PANIC("'%s' is not a valid attribute. line %d, column %d", attrName.c_str(), t->line, t->column);

					attrs.push_back(it->second);

					if ((t + 1)->type == TOKEN_COMMA) {
						expect(TOKEN_ATTR);
						continue;
					} else
						break;
				}

				parseFuncFull();
				((FunctionNode*)tree->back().get())->attrs = attrs;
			} break;

			default:
				unexpected();
			}
		}

		return std::move(root);
	}

	std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
		switch (t->type) {
		case TOKEN_INT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_INT, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			return node;
		}
		case TOKEN_FLOAT: {
			auto node = std::make_unique<ExpressionNode>(EXPR_FLOAT, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			return node;
		}
		case TOKEN_DOUBLE: {
			auto node = std::make_unique<ExpressionNode>(EXPR_DOUBLE, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			return node;
		}
		case TOKEN_STRING: {
			auto node = std::make_unique<ExpressionNode>(EXPR_STRING, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			return node;
		}
		case TOKEN_IDENTF: {
			std::vector<std::string> scope;
			std::string value = t->value;
			advance();
			while (t->type == TOKEN_SCOPE) {
				advance();
				if (t->type != TOKEN_IDENTF) unexpected();
				scope.push_back(value);
				value = t->value;
				advance();
			}
			if (t->type == TOKEN_LPAR) {
				advance();
				std::vector<std::unique_ptr<ExpressionNode>> args;
				if (t->type != TOKEN_RPAR) {
					while (true) {
						args.push_back(parseExpression());
						if (t->type == TOKEN_COMMA) {
							advance();
							continue;
						}
						break;
					}
				}
				expect(TOKEN_RPAR);
				auto node = std::make_unique<CallNode>(scope, value, std::move(args));
				node->line = t->line; node->column = t->column;
				return node;
			} else {
				auto node = std::make_unique<ExpressionNode>(EXPR_IDENTF, value);
				node->line = t->line; node->column = t->column;
				return node;
			}
		}
		case TOKEN_LPAR: {
			advance();
			auto expr = parseExpression();
			expect(TOKEN_RPAR);
			return expr;
		}
		case TOKEN_NOT: {
			advance();
			auto operand = parsePrimary();
			auto node = std::make_unique<ExpressionNode>(EXPR_NOT, "", std::move(operand));
			node->line = t->line; node->column = t->column;
			return node;
		}
		default:
			unexpected();
		}
		return nullptr;
	}

	static inline int getPrecedence(enum TokenType type) {
		switch (type) {
		case TOKEN_OR:
			return 1;
		case TOKEN_AND:
			return 2;
		case TOKEN_EQ:
		case TOKEN_NEQ:
		case TOKEN_LT:
		case TOKEN_GT:
		case TOKEN_LE:
		case TOKEN_GE:
			return 3;
		case TOKEN_ADD:
		case TOKEN_SUB:
			return 4;
		case TOKEN_MUL:
		case TOKEN_DIV:
			return 5;
		default:
			return -1;
		}
	}

	static const std::unordered_map<enum TokenType, enum ExpressionType> tt2et = {
		{TOKEN_ADD, EXPR_ADD},
		{TOKEN_SUB, EXPR_SUB},
		{TOKEN_DIV, EXPR_DIV},
		{TOKEN_MUL, EXPR_MUL},

		{TOKEN_EQ, EXPR_EQ},
		{TOKEN_NEQ, EXPR_NEQ},
		{TOKEN_LT,  EXPR_LT},
		{TOKEN_GT,  EXPR_GT},
		{TOKEN_LE,  EXPR_LE},
		{TOKEN_GE,  EXPR_GE},

		{TOKEN_NOT, EXPR_NOT},
		{TOKEN_AND, EXPR_AND},
		{TOKEN_OR, EXPR_OR},

		{TOKEN_IDENTF, EXPR_IDENTF},
	};

	std::unique_ptr<ExpressionNode> Parser::parseRhs(int exprPrec, std::unique_ptr<ExpressionNode> lhs) {
		while (true) {
			int tokPrec = getPrecedence(t->type);
			if (tokPrec < exprPrec)
				return lhs;
			enum TokenType tt = t->type;
			advance();
			auto rhs = parsePrimary();
			int nextPrec = getPrecedence(t->type);
			if (tokPrec < nextPrec)
				rhs = parseRhs(tokPrec + 1, std::move(rhs));
			lhs = std::make_unique<ExpressionNode>(tt2et.at(tt), "", std::move(lhs), std::move(rhs));
			lhs->line = (t - 1)->line; lhs->column = (t - 1)->column;
		}
	}

	std::unique_ptr<ExpressionNode> Parser::parseExpression() {
		auto lhs = parsePrimary();
		return parseRhs(0, std::move(lhs));
	}

	void Parser::parseBlock(const std::vector<enum KeywordType>& extraTerminators) {
		while (t->type != TOKEN_EOF) {
			switch (t->type) {
			case TOKEN_KWORD: {
				for (const auto& term : extraTerminators) {
					if (t->extra == term) {
						if (treeHistory.empty())
							unexpected();
						advance();
						poptree();
						return;
					}
				}

				switch (t->extra) {
				case KWORD_END: {
					if (treeHistory.empty())
						unexpected();
					advance();
					poptree();
					return;
				}

				case KWORD_RETURN: {
					advance();
					pushnode(ReturnNode, parseExpression());
					expect(TOKEN_SEMI);
					break;
				}
				case KWORD_VAR: {
					advance();
					const std::string& name = expect(TOKEN_IDENTF);
					expect(TOKEN_COLON);

					const std::string& dataType = expect(TOKEN_IDENTF);
					auto it = name2type.find(dataType);
					if (it == name2type.end())
						TEA_PANIC("unknown type '%s'. line %d, column %d", dataType.c_str(), (t - 1)->line, (t - 1)->column);

					std::unique_ptr<ExpressionNode> value = nullptr;
					if (t->type != TOKEN_SEMI) {
						expect(TOKEN_ASSIGN);
						value = parseExpression();
					}
					expect(TOKEN_SEMI);

					pushnode(VariableNode, name, it->second, std::move(value));
					break;
				}

				case KWORD_IF: {
					uint32_t line = t->line;
					uint32_t column = t->column;

					advance();
					expect(TOKEN_LPAR);
					auto pred = parseExpression();
					expect(TOKEN_RPAR);

					if (t->type != TOKEN_KWORD || t->extra != KWORD_DO)
						unexpected();
					advance();

					auto ifNode = std::make_unique<IfNode>(std::move(pred));
					ifNode->type = tnode(IfNode);
					ifNode->line = line;
					ifNode->column = column;

					treeHistory.push_back(tree);
					tree = &ifNode->body;

					parseBlock({ KWORD_ELSE, KWORD_ELSEIF });

					ElseIfNode* lastElseIf = nullptr;
					while ((t - 1)->type == TOKEN_KWORD && (t - 1)->extra == KWORD_ELSEIF) {
						expect(TOKEN_LPAR);
						auto elseIfPred = parseExpression();
						expect(TOKEN_RPAR);

						if (t->type != TOKEN_KWORD || t->extra != KWORD_DO)
							unexpected();
						advance();

						auto elseIfNode = std::make_unique<ElseIfNode>(std::move(elseIfPred));
						elseIfNode->type = tnode(ElseIfNode);
						elseIfNode->line = t->line;
						elseIfNode->column = t->column;

						treeHistory.push_back(tree);
						tree = &elseIfNode->body;

						parseBlock({ KWORD_ELSE, KWORD_ELSEIF });

						if (!lastElseIf) {
							ifNode->elseIf = std::move(elseIfNode);
							lastElseIf = ifNode->elseIf.get();
						} else {
							lastElseIf->next = std::move(elseIfNode);
							lastElseIf = lastElseIf->next.get();
						}
					}

					if ((t - 1)->type == TOKEN_KWORD && (t - 1)->extra == KWORD_ELSE) {
						auto elseNode = std::make_unique<ElseNode>();
						elseNode->type = tnode(ElseNode);
						elseNode->line = t->line;
						elseNode->column = t->column;

						treeHistory.push_back(tree);
						tree = &elseNode->body;

						parseBlock();

						ifNode->else_ = std::move(elseNode);
					}

					tree->push_back(std::move(ifNode));
				} break;

				default:
					goto unexpected;
				}
			} break;

			case TOKEN_IDENTF: {
				tree->push_back(parseExpression());
				expect(TOKEN_SEMI);
			} break;

			default:
			unexpected:
				unexpected();
			}
		}
	}

	void Parser::parseFuncFull() {
		if (t->type != TOKEN_KWORD)
			unexpected();

		enum StorageType storage;
		if (t->extra == KWORD_PUBLIC)
			storage = STORAGE_PUBLIC;
		else if (t->extra == KWORD_PRIVATE)
			storage = STORAGE_PRIVATE;
		else
			unexpected();
		advance();
		if (t->extra == KWORD_FUNC || isCC((enum KeywordType)t->extra))
			parseFunc(storage);
		else
			unexpected();
	}

	void Parser::parseFunc(enum StorageType storage) {
		enum CallingConvention cc = DEFAULT_CC;

		if (isCC((enum KeywordType)t->extra)) {
			switch (t->extra) {
			case KWORD_STDCC:	cc = CC_STD;	break;
			case KWORD_FASTCC:	cc = CC_FAST;	break;
			case KWORD_CCC:		cc = CC_C;		break;
			}
			advance();
		}
		advance();

		const std::string& name = expect(TOKEN_IDENTF);
		auto it = std::find(funcs.begin(), funcs.end(), name);
		if (it != funcs.end())
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
		funcs.push_back(name);
		expect(TOKEN_LPAR);

		std::vector<std::pair<enum Type, std::string>> args;
		if (t->type != TOKEN_RPAR) {
			do {
				const std::string& typeName = expect(TOKEN_IDENTF);
				auto typeIt = name2type.find(typeName);
				if (typeIt == name2type.end())
					TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.c_str(), (t - 1)->line, (t - 1)->column);
				const std::string& argName = expect(TOKEN_IDENTF);
				args.emplace_back(typeIt->second, argName);
			} while (t->type == TOKEN_COMMA && t++);
		}

		expect(TOKEN_RPAR);
		expect(TOKEN_ARROW);
		const std::string& returnType = expect(TOKEN_IDENTF);
		auto it2 = name2type.find(returnType);
		if (it2 == name2type.end())
			TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.c_str(), (t - 1)->line, (t - 1)->column);
		pushtree(FunctionNode, storage, cc, name, args, it2->second);
		parseBlock();
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.c_str(), t->line, t->column);
	}
}
