#include "parser.h"

#include "tea/tea.h"
#include "tea/map.h"
#include "tea/lexer/token.h"

#define pushnode(T,...) { \
	auto node = std::make_unique<T>(__VA_ARGS__); \
	node->type = tnode(T); \
	node->line = t->line; node->column = t->column; \
	tree->push(std::move(node)); \
}

#define pushtree(T,...) { \
	auto node = std::make_unique<T>(__VA_ARGS__); \
	node->type = tnode(T); \
	node->line = t->line; node->column = t->column; \
	auto body = &node->body; \
	treeHistory.push(tree); \
	tree->push(std::move(node)); \
	tree = body; \
}
#define poptree() { \
	if (TEA_UNLIKELY(treeHistory.empty())) \
		__debugbreak(); \
	tree = treeHistory[treeHistory.size-1]; \
	treeHistory.pop(); \
}

#define advance() { \
	if ((t++)->type == TOKEN_EOF) \
		TEA_PANIC("unexpected EOF. line %d, column %d", (--t)->line, t->column); \
}
#define expect(tt) _expect(t,tt)

namespace tea {
	map<string, enum Attribute> name2attr = {
		{"inline", ATTR_INLINE},
		{"noreturn", ATTR_NORETURN},
		{"nonamespace", ATTR_NONAMESPACE}
	};

	static inline const char* _expect(const Token*& t, enum TokenType expected) {
		while (t->type == TOKEN_NEWLINE)
			t++;
		if (t->type != expected)
			TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.data, t->line, t->column);
		advance();
		return (t - 1)->value;
	}

	static inline bool isCC(enum KeywordType tt) {
		return tt == KWORD_FASTCC || tt == KWORD_STDCC || tt == KWORD_CCC || tt == KWORD_AUTOCC;
	}

	Parser::Parser() {
		t = 0;
		fn = nullptr;
		tree = nullptr;
	}

	Tree Parser::parse(const vector<Token>& tokens) {
		Type::get("void"); // initializes types

		t = tokens.data;

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
					const string& name = expect(TOKEN_STRING);
					auto it = imported.find(name);;
					if (it)
						TEA_PANIC("module re-import. line %d, column %d", t->line, t->column);
					imported.push(name);
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

						const string& name = expect(TOKEN_IDENTF);
						expect(TOKEN_COLON);
						
						const string& dataType = parseType();
						auto type = Type::get(dataType);
						if (!type.second)
							TEA_PANIC("unknown type '%s'. line %d, column %d", dataType.data, (t - 1)->line, (t - 1)->column);

						std::unique_ptr<ExpressionNode> value = nullptr;
						if (t->type != TOKEN_SEMI) {
							expect(TOKEN_ASSIGN);
							value = parseExpression();
						}
						expect(TOKEN_SEMI);

						pushnode(GlobalVariableNode, storage, name, type.first, std::move(value));
					} else
						parseFuncFull();
				} break;

				case KWORD_IMPORT:
					parseFunctionImport();
					break;

				default:
					unexpected();
				} break;
			} break;

			case TOKEN_ATTR: {
				advance();
				vector<enum Attribute> attrs;
				while (true) {
					const string& attrName = expect(TOKEN_IDENTF);
					auto it = name2attr.find(attrName);
					if (!it) {
						TEA_PANIC("'%s' is not a valid attribute. line %d, column %d", attrName.data, t->line, t->column);
						TEA_UNREACHABLE();
					}

					attrs.push(*it);

					if (t->type == TOKEN_COMMA) {
						advance();
						expect(TOKEN_ATTR);
						continue;
					} else
						break;
				}

				while (t->type == TOKEN_NEWLINE)
					advance();

				if (t->extra == KWORD_IMPORT) {
					parseFunctionImport();
					((FunctionImportNode*)tree->operator[](tree->size - 1).get())->attrs = attrs;
				} else {
					parseFuncFull();
					((FunctionNode*)tree->operator[](tree->size - 1).get())->attrs = attrs;
				}
			} break;

			case TOKEN_NEWLINE:
				t++;
				break;

			default:
				unexpected();
			}
		}

		if (tree != &root)
			TEA_PANIC("unexpected EOF (did you forget to close a function?)");

		return std::move(root);
	}

	std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
		std::unique_ptr<ExpressionNode> node;

		switch (t->type) {
		case TOKEN_INT: {
			node = std::make_unique<ExpressionNode>(EXPR_INT, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			break;
		}
		case TOKEN_FLOAT: {
			node = std::make_unique<ExpressionNode>(EXPR_FLOAT, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			break;
		}
		case TOKEN_DOUBLE: {
			node = std::make_unique<ExpressionNode>(EXPR_DOUBLE, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			break;
		}
		case TOKEN_STRING: {
			node = std::make_unique<ExpressionNode>(EXPR_STRING, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			break;
		}
		case TOKEN_CHAR: {
			node = std::make_unique<ExpressionNode>(EXPR_CHAR, t->value);
			node->line = t->line; node->column = t->column;
			advance();
			break;
		}
		case TOKEN_IDENTF: {
			vector<string> scope;
			string value = t->value;
			advance();
			while (t->type == TOKEN_SCOPE) {
				advance();
				if (t->type != TOKEN_IDENTF) unexpected();
				scope.push(value);
				value = t->value;
				advance();
			}

			if (t->type == TOKEN_LPAR) {
				advance();
				vector<std::unique_ptr<ExpressionNode>> args;
				if (t->type != TOKEN_RPAR) {
					while (true) {
						args.push(parseExpression());
						if (t->type == TOKEN_COMMA) {
							advance();
							continue;
						}
						break;
					}
				}
				expect(TOKEN_RPAR);
				node = std::make_unique<CallNode>(scope, value, std::move(args));
			} else
				node = std::make_unique<ExpressionNode>(EXPR_IDENTF, value);
			break;
		}
		case TOKEN_LPAR: {
			advance();
			auto expr = parseExpression();
			expect(TOKEN_RPAR);
			node = std::move(expr);
			break;
		}
		case TOKEN_NOT: {
			advance();
			auto operand = parsePrimary();
			node = std::make_unique<ExpressionNode>(EXPR_NOT, "", std::move(operand));
			break;
		}
		case TOKEN_REF: {
			advance();
			auto operand = parsePrimary();
			node = std::make_unique<ExpressionNode>(EXPR_REF, "", std::move(operand));
			break;
		}
		case TOKEN_MUL: {
			advance();
			auto operand = parsePrimary();
			node = std::make_unique<ExpressionNode>(EXPR_DEREF, "", std::move(operand));
			break;
		}
		case TOKEN_LBRAC: {
			advance();
			vector<std::unique_ptr<ExpressionNode>> init;
			while (t->type != TOKEN_RBRAC) {
				init.push(parseExpression());
				if (t->type != TOKEN_COMMA)
					break;
				advance();
			}
			if (init.size == 0)
				TEA_PANIC("cannot create an empty array. line %d, column %d", t->line, t->column);
			advance();
			node = std::make_unique<ArrayNode>(std::move(init));
			break;
		}
		default:
			unexpected();
		}

		node->line = t->line; node->column = t->column;

		while (t->type == TOKEN_LBRAC) {
			advance();
			auto indexExpr = parseExpression();
			expect(TOKEN_RBRAC);
			node = std::make_unique<IndexNode>(std::move(node), std::move(indexExpr), true);
			node->line = t->line; node->column = t->column;
		}

		return node;
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

	static const map<enum TokenType, enum ExpressionType> tt2et = {
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
			lhs = std::make_unique<ExpressionNode>(*tt2et.find(tt), "", std::move(lhs), std::move(rhs));
			lhs->line = (t - 1)->line; lhs->column = (t - 1)->column;
		}
	}

	std::unique_ptr<ExpressionNode> Parser::parseExpression() {
		auto lhs = parsePrimary();
		return parseRhs(0, std::move(lhs));
	}

	void Parser::parseBlock(const vector<enum KeywordType>& extraTerminators) {
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
					if (t->type == TOKEN_SEMI) {
						advance();
						pushnode(ReturnNode, nullptr);
					} else {
						pushnode(ReturnNode, parseExpression());
						expect(TOKEN_SEMI);
					}
				} break;

				case KWORD_VAR: {
					advance();
					parseVariable();
				} break;

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

					treeHistory.push(tree);
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

						treeHistory.push(tree);
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

						treeHistory.push(tree);
						tree = &elseNode->body;

						parseBlock();

						ifNode->else_ = std::move(elseNode);
					}

					tree->push(std::move(ifNode));
				} break;

				case KWORD_WHILE: {
					advance();
					auto pred = parseExpression();
					pushtree(WhileLoopNode, std::move(pred));
					
					if (t->type != TOKEN_KWORD || t->extra != KWORD_DO)
						goto unexpected;
					else
						advance();

					parseBlock();
				} break;

				case KWORD_FOR: {
					advance();
					expect(TOKEN_LPAR);
					vector<VariableNode*> vars;
					while (t->extra == KWORD_VAR) {
						advance();
						parseVariable();
						vars.push((VariableNode*)tree->data[tree->size - 1].get());
					}
					auto pred = parseExpression();
					expect(TOKEN_SEMI);
					
					auto lhs = parsePrimary();
					enum TokenType extra = (TokenType)0;

					switch (t->type) {
					case TOKEN_ASSIGN:
						break;
					case TOKEN_ADD:
					case TOKEN_SUB:
					case TOKEN_MUL:
					case TOKEN_DIV:
						extra = t->type;
						advance();
						break;
					default:
						unexpected();
					}
					expect(TOKEN_ASSIGN);
					auto step = std::make_unique<AssignmentNode>(std::move(lhs), parseExpression(), extra);

					expect(TOKEN_RPAR);
					if (t->type != TOKEN_KWORD || t->extra != KWORD_DO)
						goto unexpected;
					else
						advance();
					pushtree(ForLoopNode, vars, std::move(pred), std::move(step));
					parseBlock();
				} break;

				default:
					goto unexpected;
				}
			} break;

			case TOKEN_IDENTF: {
				if (!tryParseAssignment()) {
					tree->push(parseExpression());
					expect(TOKEN_SEMI);
				} else
					expect(TOKEN_SEMI);
			} break;

			case TOKEN_MUL:
				if (!tryParseAssignment()) goto unexpected;
				else expect(TOKEN_SEMI);
				break;

			case TOKEN_SEMI:
			case TOKEN_NEWLINE:
				advance();
				break;

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
		enum CallConv cc = DEFAULT_CC;

		if (isCC((enum KeywordType)t->extra)) {
			switch (t->extra) {
			case KWORD_STDCC:	cc = CC_STD;	break;
			case KWORD_FASTCC:	cc = CC_FAST;	break;
			case KWORD_CCC:		cc = CC_C;		break;
			case KWORD_AUTOCC:	cc = CC_AUTO;	break;
			}
			advance();
		}
		advance();

		const string& name = expect(TOKEN_IDENTF);
		if (funcs.find(name))
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
		funcs.push(name);
		expect(TOKEN_LPAR);

		bool vararg = false;
		vector<std::pair<Type, string>> args;
		if (t->type != TOKEN_RPAR) {
			do {
				if (t->type == TOKEN_DOT && (t + 1)->type == TOKEN_DOT && (t + 2)->type == TOKEN_DOT) {
					advance();
					advance();
					advance();
					vararg = true;
					break;
				}

				const string& typeName = parseType();
				auto type = Type::get(typeName);
				if (!type.second)
					TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.data, (t - 1)->line, (t - 1)->column);
				const string& argName = expect(TOKEN_IDENTF);
				args.emplace(type.first, argName);
			} while (t->type == TOKEN_COMMA && t++);
		}

		expect(TOKEN_RPAR);

		Type type;
		if (t->type == TOKEN_ARROW) {
			advance();

			const string& returnType = parseType(false);
			// could this be an alex g reference 🤔
			auto [ty, success] = Type::get(returnType);
			if (!success)
				TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.data, (t - 1)->line, (t - 1)->column);
			type = ty;
		}

		Tree* oldTree = tree;
		pushtree(FunctionNode, storage, cc, name, args, type, vararg);
		fn = (FunctionNode*)oldTree->operator[](oldTree->size-1).get();

		parseBlock();
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", t->value.data, t->line, t->column);
	}

	string Parser::parseType(bool ignoreNl) {
		string typeName;
		string first = expect(TOKEN_IDENTF);

		if (first == "const") {
			typeName += "const ";
			string next = expect(TOKEN_IDENTF);
			if (next == "signed" || next == "unsigned") {
				typeName += next;
				typeName += ' ';
				typeName += expect(TOKEN_IDENTF);
			} else
				typeName += next;
		} else if (first == "signed" || first == "unsigned") {
			typeName += first;
			typeName += ' ';
			string next = expect(TOKEN_IDENTF);
			if (next == "const") {
				typeName += "const ";
				typeName += expect(TOKEN_IDENTF);
			} else
				typeName += next;
		} else
			typeName += first;

		if (ignoreNl && t->type != TOKEN_NEWLINE) {
			while (t->type == TOKEN_MUL) {
				typeName += '*';
				advance();
			}
		}

		while (t->type == TOKEN_LBRAC) {
			advance();
			typeName += '[';
			typeName += expect(TOKEN_INT); // TODO: deduct array size
			expect(TOKEN_RBRAC);
			typeName += ']';
		}

		return typeName;
	}

	bool Parser::tryParseAssignment() {
		const Token* oldt = t;
		if (t->type != TOKEN_IDENTF && t->type != TOKEN_MUL)
			return false;

		auto lhs = parsePrimary();
		enum TokenType extra = (TokenType)0;

		switch (t->type) {
			case TOKEN_ASSIGN:
				break;
			case TOKEN_ADD:
			case TOKEN_SUB:
			case TOKEN_MUL:
			case TOKEN_DIV:
				extra = t->type;
				advance();
				break;
			default:
				t = oldt;
				return false;
		}
		expect(TOKEN_ASSIGN);

		auto rhs = parseExpression();
		pushnode(AssignmentNode, std::move(lhs), std::move(rhs), extra);
		return true;
	}

	void Parser::parseFunctionImport() {
		advance();
		enum CallConv cc = DEFAULT_CC;

		if (isCC((enum KeywordType)t->extra)) {
			switch (t->extra) {
			case KWORD_STDCC:	cc = CC_STD;	break;
			case KWORD_FASTCC:	cc = CC_FAST;	break;
			case KWORD_CCC:		cc = CC_C;		break;
			case KWORD_AUTOCC:	cc = CC_AUTO;	break;
			}
			advance();
		}
		else
			unexpected();

		if (t->type != TOKEN_KWORD || t->extra != KWORD_FUNC)
			unexpected();
		advance();

		const string& name = expect(TOKEN_IDENTF);
		auto it = funcs.find(name);
		if (it)
			TEA_PANIC("function re-definition. line %d, column %d", t->line, t->column);
		funcs.push(name);
		expect(TOKEN_LPAR);

		bool vararg = false;
		vector<std::pair<Type, string>> args;
		if (t->type != TOKEN_RPAR) {
			do {
				if (t->type == TOKEN_DOT && (t + 1)->type == TOKEN_DOT && (t + 2)->type == TOKEN_DOT) {
					advance();
					advance();
					advance();
					vararg = true;
					break;
				}

				const string& typeName = parseType();
				auto type = Type::get(typeName);
				if (!type.second)
					TEA_PANIC("unknown type '%s'. line %d, column %d", typeName.data, (t - 1)->line, (t - 1)->column);
				const string& argName = expect(TOKEN_IDENTF);
				args.emplace(type.first, argName);
			} while (t->type == TOKEN_COMMA && t++);
		}

		expect(TOKEN_RPAR);
		expect(TOKEN_ARROW);
		const string& returnType = parseType();
		auto type = Type::get(returnType);
		if (!type.second)
			TEA_PANIC("unknown type '%s'. line %d, column %d", returnType.data, (t - 1)->line, (t - 1)->column);

		expect(TOKEN_SEMI);
		pushnode(FunctionImportNode, cc, name, args, type.first, vararg);
	}

	void Parser::parseVariable() {
		const string& name = expect(TOKEN_IDENTF);

		Type type;
		std::unique_ptr<ExpressionNode> value = nullptr;

		if (t->type == TOKEN_COLON) {
			advance();
			const string& dataType = parseType();
			auto pair = Type::get(dataType);
			if (!pair.second)
				TEA_PANIC("unknown type '%s'. line %d, column %d", dataType.data, (t - 1)->line, (t - 1)->column);
			type = pair.first;
		}

		if (t->type == TOKEN_ASSIGN) {
			advance();
			value = parseExpression();
		}
		expect(TOKEN_SEMI);

		pushnode(VariableNode, name, type, std::move(value));

		auto vn = (VariableNode*)tree->data[tree->size - 1].get();
		fn->prealloc.insert(vn, nullptr); // TODO: only variables inside loops should be preallocated
	}
}
